/* Cold startup/presentation helpers live in overlay 52. */
#include <arch/zxn.h>
#include <z80.h>
#include <intrinsic.h>
#include <string.h>
#include "runtime.h"
#include "platform.h"
#include "agi/picture.h"
static const uint16_t ega[16] = {
    0x000,0x005,0x028,0x02d,0x140,0x145,0x150,0x16d,
    0x092,0x097,0x0ba,0x0bf,0x1d2,0x1d7,0x1fa,0x1ff
};
/* NEX owns the machine; there is no saved NextZXOS context to return to. */
void next_exit(void) {
    intrinsic_di();
    __asm
        im 1
    __endasm;
    if(ZXN_READ_REG(0)==10)ZXN_WRITE_REG(2,2);
    next_screen_clear(0);
    strcpy(rt_buffer,"GAME ENDED");next_text(15,12,rt_buffer);
    strcpy(rt_buffer,"Reset or close the emulator.");next_text(6,14,rt_buffer);
    for(;;){}
}
void next_init(void) {
    uint8_t i;
    intrinsic_di();
    ZXN_WRITE_REG(7, 3);
    z80_outp(0xfe, 0);
    ZXN_WRITE_REG(0x12, 20);
    ZXN_WRITE_REG(0x70, 0x10);
    ZXN_WRITE_REG(0x15, 0);
    ZXN_WRITE_REG(0x16, 0); ZXN_WRITE_REG(0x17, 0); ZXN_WRITE_REG(0x71, 0);
    ZXN_WRITE_REG(0x1c, 1);
    ZXN_WRITE_REG(0x18, 0); ZXN_WRITE_REG(0x18, 159);
    ZXN_WRITE_REG(0x18, 0); ZXN_WRITE_REG(0x18, 255);
    ZXN_WRITE_REG(0x43, 0x10); ZXN_WRITE_REG(0x40, 0);
    for (i = 0; i < 16; ++i) {
        ZXN_WRITE_REG(0x44, ega[i] >> 1); ZXN_WRITE_REG(0x44, ega[i] & 1);
    }
    ZXN_WRITE_REG(0x14, 0xe3);
    for (i = 40; i < 50; ++i) {
        ZXN_WRITE_REG(0x53, i); memset((void *)0x6000, 0, 8192);
    }
    z80_outp(0x123b, 2);
}
void next_show_picture(uint8_t priority) {
    uint16_t offset;
    uint8_t page, y, x, p;
    uint8_t *source, *destination;
    for (page = 0; page < 10; ++page) {
        ZXN_WRITE_REG(0x53, 40 + page);
        offset = (uint16_t)page * 16;
        for (y = 0; y < AGI_HEIGHT; ++y, offset += AGI_WIDTH) {
            /* Both 160-byte rows and 8K boundaries align to 16 pixels. */
            agi_pixels_read(offset,rt_buffer,16);source=(uint8_t *)rt_buffer;
            destination = (uint8_t *)(0x6000u + y + next_picture_top);
            for (x = 0; x < 16; ++x) {
                p = *source++;
                p = priority ? p >> 4 : p & 15;
                *destination = p; destination[256] = p;
                destination += 512;
            }
        }
    }
}
