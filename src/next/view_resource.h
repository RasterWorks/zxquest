#ifndef NEXT_VIEW_RESOURCE_H
#define NEXT_VIEW_RESOURCE_H
#include "../agi/view.h"
#include "resource_cache.h"
/* Owns one pin. Keep at a stable resident address; reader.context points inside
   this struct, so copying an open next_view by value is not supported. */
typedef struct { resource_handle handle; agi_view_source source; } next_view;
uint8_t next_view_open(next_view *view, uint8_t number);
void next_view_close(next_view *view);
#endif
