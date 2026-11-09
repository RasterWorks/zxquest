#ifndef AGI_PICTURE_H
#define AGI_PICTURE_H
#include <stdint.h>
#define AGI_WIDTH 160
#define AGI_HEIGHT 168
#define AGI_PIXELS 26880u
enum { PIC_OK, PIC_TRUNCATED, PIC_BAD_OPCODE, PIC_BAD_ARGUMENT, PIC_FILL_FULL, PIC_UNSUPPORTED_BRUSH };
/* Each pixel packs visual colour into bits 0..3, priority into bits 4..7.
   Storage is provided by the platform, with no flat-memory assumption. */
uint8_t agi_pixel_get(uint16_t offset);
void agi_pixel_set(uint16_t offset, uint8_t value);
void agi_pixel_write(uint16_t offset, uint8_t mask, uint8_t value);
void agi_span_write(uint16_t offset, uint8_t length, uint8_t mask, uint8_t value);
/* Bulk packed-pixel transfers. Range must be within AGI_PIXELS; buffers must
   stay addressable while the platform maps its picture window. */
void agi_pixels_read(uint16_t offset, void *data, uint16_t length);
void agi_pixels_write(uint16_t offset, const void *data, uint16_t length);
void agi_picture_clear(void);
uint8_t agi_picture_decode(const uint8_t *data, uint16_t length);
#endif
