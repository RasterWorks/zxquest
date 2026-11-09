#include "logic_resource.h"
uint8_t next_logic_command(agi_vm *v, uint8_t op, const uint8_t *p) {
    uint8_t type,n=p[0],r; resource_handle h;
    switch(op) {
    case 0x18: type=RESOURCE_PICTURE; n=v->vars[n]; break;
    case 0x1e: type=RESOURCE_VIEW; break;
    case 0x1f: type=RESOURCE_VIEW; n=v->vars[n]; break;
    case 0x62: type=RESOURCE_SOUND; break;
    default: return VM_UNSUPPORTED;
    }
    r=resource_acquire(type,n,&h);
    if(r) return VM_IO;
    resource_release(h); return VM_DONE;
}
