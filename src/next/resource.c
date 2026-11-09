#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "resource.h"
uint8_t resource_open(const char *directory, const char *index, uint8_t number,
                      resource_stream *stream) {
    char path[128];
    uint8_t handle, entry[3], header[5], volume, result = RES_OK;
    uint32_t offset;
    stream->length = stream->remaining = 0;
    if (!directory || !index || strlen(directory) > 100 || strlen(index) > 20) return RES_ARGUMENT;
    sprintf(path, "%s/%s", directory, index);
    errno = 0; handle = esx_f_open(path, ESX_MODE_READ | ESX_MODE_OPEN_EXIST);
    if (errno) return RES_IO;
    esx_f_seek(handle, (uint32_t)number * 3, ESX_SEEK_SET);
    if (errno || esx_f_read(handle, entry, 3) != 3) result = RES_MISSING;
    esx_f_close(handle);
    if (result) return result;
    if ((entry[0] & 15) == 15 && entry[1] == 255 && entry[2] == 255) return RES_MISSING;
    volume = entry[0] >> 4;
    offset = ((uint32_t)(entry[0] & 15) << 16) | ((uint16_t)entry[1] << 8) | entry[2];
    sprintf(path, "%s/VOL.%u", directory, (unsigned int)volume);
    errno = 0; handle = esx_f_open(path, ESX_MODE_READ | ESX_MODE_OPEN_EXIST);
    if (errno) return RES_IO;
    esx_f_seek(handle, offset, ESX_SEEK_SET);
    if (errno || esx_f_read(handle, header, 5) != 5) result = RES_IO;
    else if (header[0] != 0x12 || header[1] != 0x34 || header[2] != volume) result = RES_HEADER;
    if (result) esx_f_close(handle);
    else {
        stream->file = handle;
        stream->length = stream->remaining = header[3] | ((uint16_t)header[4] << 8);
    }
    return result;
}
uint8_t resource_read(resource_stream *stream, void *data, uint16_t count) {
    if (count > stream->remaining) return RES_RANGE;
    if (!count) return RES_OK;
    errno = 0;
    if (esx_f_read(stream->file, data, count) != count || errno) return RES_IO;
    stream->remaining -= count;
    return RES_OK;
}
void resource_close(resource_stream *stream) { esx_f_close(stream->file); }
uint8_t resource_load(const char *directory, const char *index, uint8_t number,
                      uint8_t *data, uint16_t capacity, uint16_t *length) {
    resource_stream stream;
    uint8_t result;
    *length = 0;
    result = resource_open(directory, index, number, &stream);
    if (result) return result;
    *length = stream.length;
    result = stream.length > capacity ? RES_TOO_LARGE : resource_read(&stream, data, stream.length);
    resource_close(&stream);
    return result;
}
