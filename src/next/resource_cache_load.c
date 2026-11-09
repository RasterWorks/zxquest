#include <string.h>
#include "resource_cache_internal.h"
static const char *indices[]={"LOGDIR","PICDIR","VIEWDIR","SNDDIR"};
static void free_pages(cache_entry *entry) {
    uint8_t i;
    for (i=0; i<entry->count; ++i) cache_owners[entry->pages[i]] = 0;
    entry->count = 0;
}
static void evict(cache_entry *entry) {
    uint8_t i;
    for (i=0; i<RESOURCE_CACHE_ENTRIES; ++i)
        if (cache_entries[i].handle && cache_entries[i].age > entry->age) --cache_entries[i].age;
    free_pages(entry);
    entry->handle = 0;
    ++cache_evictions;
}
static cache_entry *oldest(void) {
    uint8_t i;
    cache_entry *entry = 0;
    for (i=0; i<RESOURCE_CACHE_ENTRIES; ++i)
        if (cache_entries[i].handle && !cache_entries[i].pins && (!entry || cache_entries[i].age > entry->age))
            entry = &cache_entries[i];
    return entry;
}
uint8_t resource_scratch(uint8_t count,uint8_t *pages) {
    uint8_t available=cache_free_count(),i,n=0;
    if(!count || count>RESOURCE_CACHE_PAGES)return RES_ARGUMENT;
    for(i=0;i<RESOURCE_CACHE_ENTRIES;++i)
        if(cache_entries[i].handle && !cache_entries[i].pins)available+=cache_entries[i].count;
    if(available<count)return RES_NO_MEMORY;
    while(cache_free_count()<count)evict(oldest());
    for(i=0;i<RESOURCE_CACHE_PAGES && n<count;++i)if(!cache_owners[i]) {
        cache_owners[i]=2;pages[n++]=RESOURCE_CACHE_FIRST_PAGE+i;
    }
    return RES_OK;
}
void resource_scratch_release(uint8_t count,const uint8_t *pages) {
    uint8_t i;
    while(count--) {
        i=*pages++-RESOURCE_CACHE_FIRST_PAGE;
        if(i<RESOURCE_CACHE_PAGES && cache_owners[i]==2)cache_owners[i]=0;
    }
}
uint8_t resource_cache_init(const char *directory) {
    if (!directory || strlen(directory) > 100) return RES_ARGUMENT;
    strcpy(cache_game_directory, directory);
    memset(cache_entries, 0, sizeof(cache_entries));
    cache_recent=0;
    memset(cache_owners, 0, sizeof(cache_owners));
    cache_hits = cache_misses = cache_evictions = 0;
    /* Keep cache_serial across resets so handles from another game remain stale. */
    return RES_OK;
}
uint8_t resource_acquire(uint8_t type, uint8_t number, resource_handle *handle) {
    uint8_t i, needed, available, result;
    uint16_t at, chunk;
    cache_entry *entry = 0, *victim;
    resource_stream stream;
    *handle = 0;
    if (type > RESOURCE_SOUND || !cache_game_directory[0]) return RES_ARGUMENT;
    if(cache_recent && cache_recent->handle && cache_recent->type==type && cache_recent->number==number)entry=cache_recent;
    else for (i=0; i<RESOURCE_CACHE_ENTRIES; ++i) {
        if (cache_entries[i].handle && cache_entries[i].type == type && cache_entries[i].number == number) {
            entry = &cache_entries[i];break;
        }
    }
    if(entry) {
        if(entry->pins==255)return RES_PIN_LIMIT;
        ++entry->pins;++cache_hits;cache_touch(entry);cache_recent=entry;*handle=entry->handle;return RES_OK;
    }
    ++cache_misses;
    result = resource_open(cache_game_directory, indices[type], number, &stream);
    if (result) return result;
    /* Never recycle a token, even after 2^32-1 loads. */
    if (cache_serial == 0xffffffffUL) { resource_close(&stream); return RES_NO_MEMORY; }
    needed = (stream.length >> 13) + ((stream.length & 8191) != 0);
    available = cache_free_count();
    for (i=0; i<RESOURCE_CACHE_ENTRIES; ++i) {
        if (!cache_entries[i].handle) entry = &cache_entries[i];
        else if (!cache_entries[i].pins) available += cache_entries[i].count;
    }
    victim = oldest();
    if (available < needed || (!entry && !victim)) {
        resource_close(&stream); return RES_NO_MEMORY;
    }
    if (!entry) { entry = victim; evict(entry); }
    while (cache_free_count() < needed) evict(oldest());
    memset(entry, 0, sizeof(*entry));
    for (i=0; i<RESOURCE_CACHE_PAGES && entry->count < needed; ++i)
        if (!cache_owners[i]) { cache_owners[i] = 1; entry->pages[entry->count++] = i; }
    at = 0;
    while (at < stream.length) {
        chunk = stream.length - at;
        if (chunk > sizeof(cache_staging)) chunk = sizeof(cache_staging);
        result = resource_read(&stream, cache_staging, chunk);
        if (result) break;
        cache_transfer(entry, at, cache_staging, chunk, 1);
        at += chunk;
    }
    resource_close(&stream);
    if (result) { free_pages(entry); return result; }
    entry->length = stream.length;
    entry->type = type; entry->number = number; entry->pins = 1;
    entry->handle = ++cache_serial; entry->age = 255; cache_touch(entry);
    cache_recent=entry;*handle = entry->handle;
    return RES_OK;
}
