#include <arch/zxn.h>
#include <string.h>
#include "resource_cache_internal.h"

cache_entry cache_entries[RESOURCE_CACHE_ENTRIES];
/* Consecutive reads normally address the same pinned LOGIC or VIEW. This
   hint is checked against the complete token, including after eviction. */
cache_entry *cache_recent;
uint8_t cache_owners[RESOURCE_CACHE_PAGES];
uint8_t cache_staging[512];
char cache_game_directory[101];
resource_handle cache_serial;
uint16_t cache_hits, cache_misses, cache_evictions;


static cache_entry *find(resource_handle handle) {
    uint8_t i;
    if(handle && cache_recent && cache_recent->handle==handle)return cache_recent;
    if (handle) for (i=0; i<RESOURCE_CACHE_ENTRIES; ++i)
        if (cache_entries[i].handle == handle) return cache_recent=&cache_entries[i];
    return 0;
}
/* Exact LRU ranks 0..31, without timestamps that wrap during long games. */
void cache_touch(cache_entry *entry) {
    uint8_t i;
    if(!entry->age)return;
    for (i=0; i<RESOURCE_CACHE_ENTRIES; ++i)
        if (cache_entries[i].handle && cache_entries[i].age < entry->age) ++cache_entries[i].age;
    entry->age = 0;
}
uint8_t cache_free_count(void) {
    uint8_t i, count = 0;
    for (i=0; i<RESOURCE_CACHE_PAGES; ++i) if (!cache_owners[i]) ++count;
    return count;
}
/* This never calls the filesystem while banked memory is mapped. */
void cache_transfer(cache_entry *entry, uint16_t offset, uint8_t *data, uint16_t count, uint8_t write) {
    uint8_t saved;
    uint16_t chunk;
    uint8_t *window;
    if (!count) return;
    saved = ZXN_READ_REG(0x53);
    while (count) {
        chunk = 8192 - (offset & 8191);
        if (chunk > count) chunk = count;
        ZXN_WRITE_REG(0x53, RESOURCE_CACHE_FIRST_PAGE + entry->pages[offset >> 13]);
        window = (uint8_t *)(0x6000u + (offset & 8191));
        if (write) memcpy(window, data, chunk); else memcpy(data, window, chunk);
        data += chunk; offset += chunk; count -= chunk;
    }
    ZXN_WRITE_REG(0x53, saved);
}
uint8_t resource_release(resource_handle handle) {
    cache_entry *entry = find(handle);
    if (!entry) return RES_STALE;
    if (!entry->pins) return RES_ARGUMENT;
    --entry->pins;
    return RES_OK;
}
uint8_t resource_size(resource_handle handle, uint16_t *length) {
    cache_entry *entry = find(handle);
    *length = 0;
    if (!entry) return RES_STALE;
    *length = entry->length;
    return RES_OK;
}
uint8_t resource_copy(resource_handle handle, uint16_t offset, void *data, uint16_t count) {
    cache_entry *entry = find(handle);
    if (!entry) return RES_STALE;
    /* Subtraction avoids 16-bit offset+count overflow. */
    if (offset > entry->length || count > entry->length - offset) return RES_RANGE;
    cache_transfer(entry, offset, data, count, 0);
    cache_touch(entry);
    return RES_OK;
}
