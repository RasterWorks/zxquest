/* Brush patterns/placement adapted from Sarien picture.c.
 * Copyright (C) 1999-2001 Stuart George and Claudio Matsuoka.
 * GNU General Public License v2; see COPYING.
 */
#include "picture.h"

static const uint8_t *stream;
static uint16_t cursor, limit;
static uint8_t visual, priority, colour, pri_colour, failure;
static uint8_t draw_mask, draw_value;
static uint16_t pending[512];
static uint16_t count;
static uint8_t fill_row[AGI_WIDTH];

static const uint8_t circles[][15] = {		/* agi circle bitmaps */
	{ 0x80 },
	{ 0xfc },
	{ 0x5f, 0xf4 },
	{ 0x66, 0xff, 0xf6, 0x60 },
	{ 0x23, 0xbf, 0xff, 0xff, 0xee, 0x20 },
	{ 0x31, 0xe7, 0x9e, 0xff, 0xff, 0xde, 0x79, 0xe3, 0x00 },
	{ 0x38, 0xf9, 0xf3, 0xef, 0xff, 0xff,
	  0xff, 0xfe, 0xf9, 0xf3, 0xe3, 0x80 },
	{ 0x18, 0x3c, 0x7e, 0x7e, 0x7e, 0xff, 0xff,
	  0xff, 0xff, 0xff, 0x7e, 0x7e, 0x7e, 0x3c, 0x18 }
};

static const uint8_t splatter_map[32] = {	/* splatter brush bitmaps */
	0x20, 0x94, 0x02, 0x24, 0x90, 0x82, 0xa4, 0xa2,
	0x82, 0x09, 0x0a, 0x22, 0x12, 0x10, 0x42, 0x14,
	0x91, 0x4a, 0x91, 0x11, 0x08, 0x12, 0x25, 0x10,
	0x22, 0xa8, 0x14, 0x24, 0x00, 0x50, 0x24, 0x04
};

static const uint8_t splatter_start[128] = {	/* starting bit position */
	0x00, 0x18, 0x30, 0xc4, 0xdc, 0x65, 0xeb, 0x48,
	0x60, 0xbd, 0x89, 0x05, 0x0a, 0xf4, 0x7d, 0x7d,
	0x85, 0xb0, 0x8e, 0x95, 0x1f, 0x22, 0x0d, 0xdf,
	0x2a, 0x78, 0xd5, 0x73, 0x1c, 0xb4, 0x40, 0xa1,
	0xb9, 0x3c, 0xca, 0x58, 0x92, 0x34, 0xcc, 0xce,
	0xd7, 0x42, 0x90, 0x0f, 0x8b, 0x7f, 0x32, 0xed,
	0x5c, 0x9d, 0xc8, 0x99, 0xad, 0x4e, 0x56, 0xa6,
	0xf7, 0x68, 0xb7, 0x25, 0x82, 0x37, 0x3a, 0x51,
	0x69, 0x26, 0x38, 0x52, 0x9e, 0x9a, 0x4f, 0xa7,
	0x43, 0x10, 0x80, 0xee, 0x3d, 0x59, 0x35, 0xcf,
	0x79, 0x74, 0xb5, 0xa2, 0xb1, 0x96, 0x23, 0xe0,
	0xbe, 0x05, 0xf5, 0x6e, 0x19, 0xc5, 0x66, 0x49,
	0xf0, 0xd1, 0x54, 0xa9, 0x70, 0x4b, 0xa4, 0xe2,
	0xe6, 0xe5, 0xab, 0xe4, 0xd2, 0xaa, 0x4c, 0xe3,
	0x06, 0x6f, 0xc6, 0x4a, 0xa4, 0x75, 0x97, 0xe1
};

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
/* AGI brushes are half-width in picture coordinates. Clip painted points,
   rather than rejecting a valid stamp whose footprint crosses a screen edge. */
static void stamp(int x,int y,uint8_t brush,uint8_t pattern) {
    uint8_t size=brush&7,bit=0,texture=splatter_start[pattern],paint;
    int xx,yy,left,right,bottom;
    if(x<size)x=size-1;if(y<size)y=size;
    left=x-(size+1)/2;right=x+size/2;bottom=y+size;
    for(yy=y-size;yy<=bottom;++yy)for(xx=left;xx<=right;++xx){
        paint=brush&0x10;
        if(!paint)paint=circles[size][bit>>3]&(0x80>>(bit&7));
        if(paint){
            if(brush&0x20){paint=splatter_map[texture>>3]&(0x80>>(texture&7));if(++texture==255)texture=0;}
            if(paint && xx>=0 && xx<AGI_WIDTH && yy>=0 && yy<AGI_HEIGHT)plot(xx,yy);
        }
        ++bit;
    }
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
    uint8_t op, value, axis, brush = 0, pattern;
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
        case 0xf8:
            while (more() && !failure) {
                x = argument(); y = argument();
                if (failure) break;
                if (x >= AGI_WIDTH || y >= AGI_HEIGHT) { failure = PIC_BAD_ARGUMENT; break; }
                fill(x, y);
            }
            break;
        case 0xfa:
            while(more() && !failure){
                pattern=(brush&0x20)?argument()>>1:0;
                x=argument();y=argument();
                if(!failure)stamp(x,y,brush,pattern);
            }
            break;
        case 0xf9: brush = argument(); break;
        default: failure = PIC_BAD_OPCODE;
        }
    }
    return failure ? failure : PIC_TRUNCATED;
}
