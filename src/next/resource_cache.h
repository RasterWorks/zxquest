#ifndef NEXT_RESOURCE_CACHE_H
#define NEXT_RESOURCE_CACHE_H
#include "resource.h"

enum { RESOURCE_LOGIC, RESOURCE_PICTURE, RESOURCE_VIEW, RESOURCE_SOUND };
#define RESOURCE_CACHE_FIRST_PAGE 64
#define RESOURCE_CACHE_PAGES 24
#define RESOURCE_CACHE_ENTRIES 32
typedef uint32_t resource_handle;
typedef struct {
    uint16_t hits, misses, evictions;
    uint8_t pages_used, entries, pinned;
} resource_cache_info;

/* Single game, synchronous/main-thread use. Init invalidates every handle.
   Acquire pins a resource; each successful acquire needs one release.
   Released resources remain cached but may be evicted. Zero is never a handle.
   Data remains raw (including encrypted LOGIC text). No mapped pointers escape.
   All buffers must be resident at $8000 or above; MMU2/3 are preserved.
   Do not call from an ISR. An interrupt may temporarily map MMU3 only if it
   restores both MMU3 and the NextReg selector before returning; sound.c does
   this without touching cache state, MMU2 or calling the filesystem. */
uint8_t resource_cache_init(const char *directory);
uint8_t resource_acquire(uint8_t type, uint8_t number, resource_handle *handle);
uint8_t resource_release(resource_handle handle);
uint8_t resource_size(resource_handle handle, uint16_t *length);
uint8_t resource_copy(resource_handle handle, uint16_t offset, void *data, uint16_t count);
void resource_cache_status(resource_cache_info *info);
/* Temporarily reserve physical pages, evicting only unpinned resources.
   Failure leaves all owners unchanged. Release before reinitializing cache. */
uint8_t resource_scratch(uint8_t count,uint8_t *pages);
void resource_scratch_release(uint8_t count,const uint8_t *pages);
/* Status counters are diagnostic uint16_t values and wrap; LRU does not. */
#endif
