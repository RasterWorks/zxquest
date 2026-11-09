#ifndef NEXT_PLATFORM_H
#define NEXT_PLATFORM_H
#include <stdint.h>
void next_init(void);
void next_show_picture(uint8_t priority);
void next_text(uint8_t col, uint8_t row, const char *text);
void next_console(void);
void next_screen_clear(uint8_t colour);
void next_agi_char(uint8_t col,uint8_t row,uint8_t c,uint8_t fg,uint8_t bg);
extern uint8_t next_picture_top;
/* Snapshot the clean picture into pages 36..39 before drawing any sprites. */
void next_scene_capture(void);
/* Half-open, clipped AGI pixel rectangle, upper-left based. */
void next_scene_restore(int16_t x, int16_t y, uint16_t width, uint16_t height);
void next_show_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t priority);
#endif
