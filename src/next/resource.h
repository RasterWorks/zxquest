#ifndef NEXT_RESOURCE_H
#define NEXT_RESOURCE_H
#include <stdint.h>
enum { RES_OK, RES_MISSING, RES_IO, RES_HEADER, RES_TOO_LARGE,
       RES_ARGUMENT, RES_NO_MEMORY, RES_STALE, RES_RANGE, RES_PIN_LIMIT };
/* Streams own an esxDOS handle. Call close exactly once after a successful open. */
typedef struct { uint8_t file; uint16_t length, remaining; } resource_stream;
uint8_t resource_open(const char *directory, const char *index, uint8_t number,
                      resource_stream *stream);
uint8_t resource_read(resource_stream *stream, void *data, uint16_t count);
void resource_close(resource_stream *stream);
uint8_t resource_load(const char *directory, const char *index, uint8_t number,
                      uint8_t *data, uint16_t capacity, uint16_t *length);
#endif
