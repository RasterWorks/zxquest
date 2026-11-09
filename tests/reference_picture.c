/* Frozen pre-optimization decoder, used only for pixel-regression checks. */
#include "agi/picture.h"
#define agi_picture_decode agi_picture_decode_reference

static const uint8_t *stream;
static uint16_t cursor, limit;
static uint8_t visual, priority, colour, pri_colour, failure;
static uint16_t pending[512];
static uint16_t count;

static uint8_t argument(void) {
    if (cursor >= limit || stream[cursor] >= 0xf0) {
        failure = PIC_TRUNCATED;
        return 0;
    }
    return stream[cursor++];
}
static uint8_t more(void) { return cursor < limit && stream[cursor] < 0xf0; }
static void plot(int x, int y) {
    uint16_t at;
    uint8_t old;
    if (x < 0 || x >= AGI_WIDTH || y < 0 || y >= AGI_HEIGHT) {
        failure = PIC_BAD_ARGUMENT;
        return;
    }
    at = (uint16_t)y * AGI_WIDTH + x;
    old = agi_pixel_get(at);
    if (visual) old = (old & 0xf0) | colour;
    if (priority) old = (old & 15) | (pri_colour << 4);
    agi_pixel_set(at, old);
}
/* Integer DDA: midpoint ties advance in the direction of travel. */
static void line(int x, int y, int end_x, int end_y) {
    int dx = end_x - x, dy = end_y - y;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    int error, steps;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    plot(x, y);
    if (dx >= dy) {
        error = dx;
        for (steps = 0; steps < dx; ++steps) {
            x += sx; error += dy * 2;
            if (error >= dx * 2) { y += sy; error -= dx * 2; }
            plot(x, y);
        }
    } else {
        error = dy;
        for (steps = 0; steps < dy; ++steps) {
            y += sy; error += dx * 2;
            if (error >= dy * 2) { x += sx; error -= dy * 2; }
            plot(x, y);
        }
    }
}
static uint8_t fillable(int x, int y) {
    uint8_t p;
    if (x < 0 || x >= AGI_WIDTH || y < 0 || y >= AGI_HEIGHT) return 0;
    p = agi_pixel_get((uint16_t)y * AGI_WIDTH + x);
    if (visual) return colour != 15 && (p & 15) == 15;
    return priority && pri_colour != 4 && (p >> 4) == 4;
}
static void push(int x, int y) {
    if (count == 512) { failure = PIC_FILL_FULL; return; }
    pending[count++] = ((uint16_t)y << 8) | x;
}
/* Scanline fill keeps the CPU stack bounded; duplicate seeds are harmless. */
static void fill(int x, int y) {
    int left, right, scan, row;
    uint16_t seed;
    count = 0;
    push(x, y);
    while (count && !failure) {
        seed = pending[--count]; x = seed & 255; y = seed >> 8;
        if (!fillable(x, y)) continue;
        left = right = x;
        while (fillable(left - 1, y)) --left;
        while (fillable(right + 1, y)) ++right;
        for (scan = left; scan <= right; ++scan) plot(scan, y);
        for (row = y - 1; row <= y + 1; row += 2) {
            scan = left;
            while (scan <= right) {
                if (fillable(scan, row)) {
                    push(scan, row);
                    do { ++scan; } while (scan <= right && fillable(scan, row));
                } else ++scan;
            }
        }
    }
}
uint8_t agi_picture_decode(const uint8_t *data, uint16_t length) {
    uint8_t op, value, axis, brush = 0;
    int x, y, nx, ny;
    stream = data; cursor = 0; limit = length;
    failure = visual = priority = colour = pri_colour = 0;
    while (cursor < limit && !failure) {
        op = stream[cursor++];
        switch (op) {
        case 0xff: return PIC_OK;
        case 0xf0: colour = argument(); visual = 1; if (colour > 15) failure = PIC_BAD_ARGUMENT; break;
        case 0xf1: visual = 0; break;
        case 0xf2: pri_colour = argument(); priority = 1; if (pri_colour > 15) failure = PIC_BAD_ARGUMENT; break;
        case 0xf3: priority = 0; break;
        case 0xf4: case 0xf5: case 0xf6: case 0xf7:
            x = argument(); y = argument();
            if (failure) break;
            plot(x, y); axis = op == 0xf5;
            while (more() && !failure) {
                nx = x; ny = y; value = argument();
                if (op == 0xf4 || op == 0xf5) {
                    if (axis) nx = value; else ny = value;
                    axis ^= 1;
                } else if (op == 0xf6) { nx = value; ny = argument(); }
                else {
                    nx += (value & 0x80) ? -((value >> 4) & 7) : ((value >> 4) & 7);
                    ny += (value & 8) ? -(value & 7) : (value & 7);
                }
                if (!failure) line(x, y, nx, ny);
                x = nx; y = ny;
            }
            break;
        case 0xf8: case 0xfa:
            if (op == 0xfa && brush) { failure = PIC_UNSUPPORTED_BRUSH; break; }
            while (more() && !failure) {
                x = argument(); y = argument();
                if (failure) break;
                if (x >= AGI_WIDTH || y >= AGI_HEIGHT) { failure = PIC_BAD_ARGUMENT; break; }
                if (op == 0xf8) fill(x, y); else plot(x, y);
            }
            break;
        case 0xf9: brush = argument(); break;
        default: failure = PIC_BAD_OPCODE;
        }
    }
    return failure ? failure : PIC_TRUNCATED;
}
