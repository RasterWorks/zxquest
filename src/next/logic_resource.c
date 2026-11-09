#include "logic_resource.h"
static uint8_t open_logic(uint8_t n, uint32_t *h, uint16_t *size) {
    uint8_t r=resource_acquire(RESOURCE_LOGIC,n,h);
    if(r) return r;
    r=resource_size(*h,size);
    if(r) resource_release(*h);
    return r;
}
static void close_logic(uint32_t h) { resource_release(h); }
const agi_logic_io next_logic_io={open_logic,close_logic,resource_copy};
