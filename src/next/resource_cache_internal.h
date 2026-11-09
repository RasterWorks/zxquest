#ifndef NEXT_CACHE_INTERNAL_H
#define NEXT_CACHE_INTERNAL_H
#include "resource_cache.h"
typedef struct {
    resource_handle handle;
    uint16_t length;
    uint8_t type, number, pins, age, count, pages[8];
} cache_entry;
extern cache_entry cache_entries[RESOURCE_CACHE_ENTRIES],*cache_recent;
extern uint8_t cache_owners[RESOURCE_CACHE_PAGES],cache_staging[512];
extern char cache_game_directory[101];
extern resource_handle cache_serial;
extern uint16_t cache_hits,cache_misses,cache_evictions;
void cache_touch(cache_entry *entry);
void cache_transfer(cache_entry *entry,uint16_t offset,uint8_t *data,uint16_t count,uint8_t write);
uint8_t cache_free_count(void);
#endif
