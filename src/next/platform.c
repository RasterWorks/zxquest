#include <arch/zxn.h>
#include <z80.h>
#include <string.h>
#include <intrinsic.h>
#include "platform.h"
#include "../agi/picture.h"

/* 8K pages 32..35: packed picture; 40..49: 320x256 Layer 2.
   CPU $4000: picture window. $6000: display window. Code stays >=$8000. */
static uint8_t mapped = 255;
uint8_t next_picture_top = 40;
#ifndef BANKED_PLATFORM
static const uint16_t ega[16] = {
    0x000,0x005,0x028,0x02d,0x140,0x145,0x150,0x16d,
    0x092,0x097,0x0ba,0x0bf,0x1d2,0x1d7,0x1fa,0x1ff
};
#endif
static uint8_t *pixel_address(uint16_t offset) {
    uint8_t page = 32 + (offset >> 13);
    if (page != mapped) { ZXN_WRITE_REG(0x52, page); mapped = page; }
    return (uint8_t *)(0x4000u + (offset & 8191));
}
uint8_t agi_pixel_get(uint16_t offset) { return *pixel_address(offset); }
void agi_pixel_set(uint16_t offset, uint8_t value) { *pixel_address(offset) = value; }
void agi_pixels_read(uint16_t offset, void *data, uint16_t length) {
    uint16_t chunk; uint8_t *p,*out=data;
    while(length) {
        chunk=8192-(offset&8191); if(chunk>length) chunk=length;
        p=pixel_address(offset); memcpy(out,p,chunk);
        offset+=chunk; out+=chunk; length-=chunk;
    }
}
void agi_pixels_write(uint16_t offset, const void *data, uint16_t length) {
    uint16_t chunk; uint8_t *p; const uint8_t *in=data;
    while(length) {
        chunk=8192-(offset&8191); if(chunk>length) chunk=length;
        p=pixel_address(offset); memcpy(p,in,chunk);
        offset+=chunk; in+=chunk; length-=chunk;
    }
}
void agi_pixel_write(uint16_t offset, uint8_t mask, uint8_t value) {
    uint8_t *p = pixel_address(offset);
    *p = (*p & (uint8_t)~mask) | (value & mask);
}
void agi_span_write(uint16_t offset, uint8_t length, uint8_t mask, uint8_t value) {
    uint8_t *p, chunk, n;
    uint16_t available;
    value &= mask;
    while (length) {
        p = pixel_address(offset);
        available = 8192 - (offset & 8191);
        chunk = available < length ? available : length;
        if (mask == 255) memset(p, value, chunk);
        else for (n = 0; n < chunk; ++n) p[n] = (p[n] & (uint8_t)~mask) | value;
        offset += chunk; length -= chunk;
    }
}
void agi_picture_clear(void) {
    uint8_t page;
    for (page = 32; page < 36; ++page) {
        ZXN_WRITE_REG(0x52, page); memset((void *)0x4000, 0x4f, 8192);
    }
    mapped = 35;
}
void next_console(void) {
    ZXN_WRITE_REG(0x52, 10); mapped = 255;
    ZXN_WRITE_REG(0x53, 11);
    z80_outp(0x123b, 0);
}
#ifndef BANKED_PLATFORM
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
static void display_pixel(uint16_t x, uint8_t y, uint8_t colour) {
    /* In 320 mode each 8K page holds 32 full-height columns. */
    ZXN_WRITE_REG(0x53, 40 + (x >> 5));
    *((uint8_t *)(0x6000u + ((x & 31) << 8) + y)) = colour;
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
            source = pixel_address(offset);
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
/* ROM font is available in direct-load CSpect and NextZXOS NEX launches. */
void next_text(uint8_t col, uint8_t row, const char *text) {
    static const uint8_t caret[8]={0,16,40,68,0,0,0,0};
    uint8_t bits, x, y, c;
    while ((c = *text++) != 0 && col < 40) {
        if (c < 32 || c > 127) c = '?';
        if (c == 96) c = 39;
        for (y = 0; y < 8; ++y) {
            bits = c==94 ? caret[y] : *((const uint8_t *)(0x3d00u + (uint16_t)(c - 32) * 8 + y));
            for (x = 0; x < 8; ++x)
                display_pixel((uint16_t)col * 8 + x, row * 8 + y, bits & (128 >> x) ? 15 : 0);
        }
        ++col;
    }
}

#endif
