#ifndef AGI_VIEW_H
#define AGI_VIEW_H
#include <stdint.h>

enum { VIEW_OK, VIEW_TRUNCATED, VIEW_BAD_HEADER, VIEW_BAD_INDEX,
       VIEW_BAD_RLE, VIEW_SOURCE_ERROR, VIEW_BAD_ARGUMENT };
/* Reader returns zero on success. Context, buffers and callback code must remain
   resident. The caller owns/pins the source for the lifetime of each operation. */
typedef struct {
    void *context;
    uint16_t length;
    uint8_t (*read)(void *context, uint16_t offset, void *data, uint16_t count);
} agi_view_source;
typedef struct { uint8_t loops; uint16_t description; } agi_view_info;
typedef struct {
    uint16_t offset;
    uint8_t loops, cels, width, height, transparent, mirrored;
} agi_view_cel;
typedef void (*agi_view_row)(void *context, uint8_t row, const uint8_t *pixels, uint8_t width);

uint8_t agi_view_inspect(const agi_view_source *source, agi_view_info *info);
uint8_t agi_view_select(const agi_view_source *source, uint8_t loop, uint8_t cel, agi_view_cel *selected);
/* Emits top-to-bottom rows, including implicit transparent pixels and mirroring.
   A null callback validates only. Shared scratch storage: not reentrant/ISR safe.
   Callbacks must not recursively decode; errors can follow earlier callbacks. */
uint8_t agi_view_decode(const agi_view_source *source, const agi_view_cel *cel, agi_view_row row, void *context);
/* x is left edge; baseline is bottom row. Clips to 160x168. Validates the entire
   cel before drawing, so malformed RLE cannot leave a partly drawn sprite. */
uint8_t agi_view_blit(const agi_view_source *source, const agi_view_cel *cel, int16_t x, int16_t baseline, uint8_t priority);
uint8_t agi_view_priority(uint8_t baseline);
#endif
