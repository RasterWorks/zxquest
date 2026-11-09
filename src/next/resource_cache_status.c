#include "resource_cache_internal.h"
void resource_cache_status(resource_cache_info *info) {
    uint8_t i;
    info->hits = cache_hits; info->misses = cache_misses; info->evictions = cache_evictions;
    info->pages_used = RESOURCE_CACHE_PAGES - cache_free_count();
    info->entries = info->pinned = 0;
    for (i=0; i<RESOURCE_CACHE_ENTRIES; ++i) if (cache_entries[i].handle) {
        ++info->entries; if (cache_entries[i].pins) ++info->pinned;
    }
}
