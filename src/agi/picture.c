#include "picture.h"

static const uint8_t *stream;
static uint16_t cursor, limit;
static uint8_t visual, priority, colour, pri_colour, failure;
static uint8_t draw_mask, draw_value;
static uint16_t pending[512];
static uint16_t count;
static uint8_t fill_row[AGI_WIDTH];

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
    if (x < 0 || x >= AGI_WIDTH || y < 0 || y >= AGI_HEIGHT) {
        failure = PIC_BAD_ARGUMENT;
        return;
    }
    at = (uint16_t)y * AGI_WIDTH + x;
    agi_pixel_write(at, draw_mask, draw_value);
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
static void push(int x, int y) {
    if (count == 512) { failure = PIC_FILL_FULL; return; }
    pending[count++] = ((uint16_t)y << 8) | x;
}
/* Scanline fill keeps the CPU stack bounded; duplicate seeds are harmless. */
static void fill(int x, int y) {
    int left, right, scan, row;
    uint16_t seed,at;
    uint8_t mask=visual ? 15 : 240, target=visual ? 15 : 64;
    if(visual ? colour==15 : !priority || pri_colour==4) return;
    count = 0;
    push(x, y);
    while (count && !failure) {
        seed = pending[--count]; x = seed & 255; y = seed >> 8;
        at=(uint16_t)y*AGI_WIDTH;
        agi_pixels_read(at,fill_row,AGI_WIDTH);
        if ((fill_row[x]&mask)!=target) continue;
        left = right = x;
        while (left>0 && (fill_row[left-1]&mask)==target) --left;
        while (right<AGI_WIDTH-1 && (fill_row[right+1]&mask)==target) ++right;
        agi_span_write(at+left, right-left+1, draw_mask, draw_value);
        for (row = y - 1; row <= y + 1; row += 2) {
            if(row<0 || row>=AGI_HEIGHT) continue;
            agi_pixels_read((uint16_t)row*AGI_WIDTH,fill_row,AGI_WIDTH);
            scan = left;
            while (scan <= right) {
                if ((fill_row[scan]&mask)==target) {
                    push(scan, row);
                    do { ++scan; } while (scan <= right && (fill_row[scan]&mask)==target);
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
    draw_mask = draw_value = 0;
    while (cursor < limit && !failure) {
        op = stream[cursor++];
        switch (op) {
        case 0xff: return PIC_OK;
        case 0xf0: colour = argument(); visual = 1; draw_mask |= 15; draw_value = (draw_value & 240) | colour; if (colour > 15) failure = PIC_BAD_ARGUMENT; break;
        case 0xf1: visual = 0; draw_mask &= 240; break;
        case 0xf2: pri_colour = argument(); priority = 1; draw_mask |= 240; draw_value = (draw_value & 15) | (pri_colour << 4); if (pri_colour > 15) failure = PIC_BAD_ARGUMENT; break;
        case 0xf3: priority = 0; draw_mask &= 15; break;
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
