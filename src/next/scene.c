#include <arch/zxn.h>
#include <string.h>
#include "platform.h"
#include "../agi/picture.h"

/* Each upload fragment is confined to 16 AGI pixels / one Layer 2 bank. */
static uint8_t row_buffer[16],row_size;
/* Fastcall supplies the first screen column in HL. A Layer 2 column is 256
   bytes, so two INC H steps duplicate one AGI pixel and advance to the next.
   BC/DE/HL/AF are caller-saved; IX/IY and the MMU mappings stay untouched.
   row_size is always 1..16 and IRQ code never uses this shared scratch. */
static void visual_row(uint8_t *destination) __z88dk_fastcall __naked {
    (void)destination;
__asm
    ld de,_row_buffer
    ld a,(_row_size)
    ld b,a
visual_row_loop:
    ld a,(de)
    inc de
    and 15
    ld (hl),a
    inc h
    ld (hl),a
    inc h
    djnz visual_row_loop
    ret
__endasm;
}
static void priority_row(uint8_t *destination) __z88dk_fastcall __naked {
    (void)destination;
__asm
    ld de,_row_buffer
    ld a,(_row_size)
    ld b,a
priority_row_loop:
    ld a,(de)
    inc de
    rrca
    rrca
    rrca
    rrca
    and 15
    ld (hl),a
    inc h
    ld (hl),a
    inc h
    djnz priority_row_loop
    ret
__endasm;
}
/* Bounds are computed in 32 bits so callers cannot wrap a 16-bit rectangle. */
static uint8_t clip(int16_t *x, int16_t *y, uint16_t *width, uint16_t *height) {
    int32_t right=(int32_t)*x+*width, bottom=(int32_t)*y+*height;
    if(right>AGI_WIDTH) right=AGI_WIDTH;
    if(bottom>AGI_HEIGHT) bottom=AGI_HEIGHT;
    if(*x<0) *x=0;
    if(*y<0) *y=0;
    if(right<=*x || bottom<=*y) return 0;
    *width=right-*x; *height=bottom-*y;
    return 1;
}
void next_scene_capture(void) {
    uint16_t at, chunk;
    uint8_t saved=ZXN_READ_REG(0x53);
    for(at=0; at<AGI_PIXELS; at+=chunk) {
        chunk=8192-(at&8191);
        if(chunk>AGI_PIXELS-at) chunk=AGI_PIXELS-at;
        ZXN_WRITE_REG(0x53,36+(at>>13));
        agi_pixels_read(at,(void *)(0x6000u+(at&8191)),chunk);
    }
    ZXN_WRITE_REG(0x53,saved);
}
void next_scene_restore(int16_t x, int16_t y, uint16_t width, uint16_t height) {
    uint16_t at, count, chunk;
    uint8_t saved;
    if(!clip(&x,&y,&width,&height)) return;
    saved=ZXN_READ_REG(0x53);
    while(height--) {
        at=(uint16_t)y++*AGI_WIDTH+x; count=width;
        while(count) {
            chunk=8192-(at&8191); if(chunk>count) chunk=count;
            ZXN_WRITE_REG(0x53,36+(at>>13));
            agi_pixels_write(at,(void *)(0x6000u+(at&8191)),chunk);
            at+=chunk; count-=chunk;
        }
    }
    ZXN_WRITE_REG(0x53,saved);
}
void next_show_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t priority) {
    uint16_t at,column,remaining,row;
    uint8_t saved,n;
    uint8_t *dest;
    if(!clip(&x,&y,&width,&height)) return;
    saved=ZXN_READ_REG(0x53);
    column=x;remaining=width;
    while(remaining) {
        n=16-(column&15);if(n>remaining)n=remaining;
        ZXN_WRITE_REG(0x53,40+(column>>4));
        at=(uint16_t)y*AGI_WIDTH+column;
        row_size=n;
        dest=(uint8_t *)(0x6000u+((column&15)<<9)+y+next_picture_top);
        for(row=0;row<height;++row) {
            /* A row fragment stays inside one picture bank and one display
               bank. Increment destinations instead of recomputing per pixel. */
            agi_pixels_read(at,row_buffer,n);at+=AGI_WIDTH;
            if(priority)priority_row(dest);else visual_row(dest);
            ++dest;
        }
        column+=n;remaining-=n;
    }
    ZXN_WRITE_REG(0x53,saved);
}
