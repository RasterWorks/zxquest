/* Console font helpers; the resident gate stages caller-owned strings. */
#include <arch/zxn.h>
#include <string.h>
#include "platform.h"
static void display_pixel(uint16_t x, uint8_t y, uint8_t colour) {
    /* In 320 mode each 8K page holds 32 full-height columns. */
    ZXN_WRITE_REG(0x53, 40 + (x >> 5));
    *((uint8_t *)(0x6000u + ((x & 31) << 8) + y)) = colour;
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
