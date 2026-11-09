#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "runtime.h"
#include "platform.h"
#include "agi/picture.h"
extern void rt_text_mask(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
agi_vm rt_vm;
rt_state rt;
char rt_buffer[768];
void rt_actor_read(uint8_t id,rt_actor *a) {
    uint8_t saved=ZXN_READ_REG(0x53);
    ZXN_WRITE_REG(0x53,60); memcpy(a,(void *)(0x6000u+(uint16_t)id*32),sizeof(*a));
    ZXN_WRITE_REG(0x53,saved);
}
void rt_actor_write(uint8_t id,const rt_actor *a) {
    uint8_t saved=ZXN_READ_REG(0x53);
    ZXN_WRITE_REG(0x53,60); memcpy((void *)(0x6000u+(uint16_t)id*32),a,sizeof(*a));
    ZXN_WRITE_REG(0x53,saved); rt.occupied[id>>3]|=1u<<(id&7);
}
uint8_t rt_picture(uint8_t id,uint8_t overlay) {
    resource_handle h; uint16_t size,at,n; uint8_t r,saved;
    r=resource_acquire(RESOURCE_PICTURE,id,&h); if(r) return VM_IO;
    resource_size(h,&size);
    if(size>8192) { resource_release(h); return VM_HEADER; }
    saved=ZXN_READ_REG(0x53);
    for(at=0;at<size;at+=n) {
        n=size-at; if(n>sizeof(rt_buffer)) n=sizeof(rt_buffer);
        r=resource_copy(h,at,rt_buffer,n); if(r) break;
        ZXN_WRITE_REG(0x53,63); memcpy((void *)(0x6000u+at),rt_buffer,n);
    }
    resource_release(h);
    if(!r) {
        if(!overlay) agi_picture_clear();
        ZXN_WRITE_REG(0x53,63); r=agi_picture_decode((const uint8_t *)0x6000,size);
    }
    ZXN_WRITE_REG(0x53,saved);
    if(r) return VM_HEADER;
    next_scene_capture(); rt.picture_valid=1; rt.picture_visible=0; rt.last_picture=id; rt.dirty=3;
    return VM_DONE;
}
void rt_redraw(void) {
    /* Rebuild graphics/text mode, including margins outside the picture.
       Menus restore their own backing store instead of using this path. */
    next_screen_clear(rt.bg);
    if(rt.gfx && rt.picture_valid && rt.picture_visible) next_show_picture(0);
    rt_text_redraw();
}
/* Bank-private: called by runtime.c in overlay 53. show.pic overwrites
   display text within the picture, but leaves status/input margins alone. */
void rt_show_picture(void) {
    uint8_t saved=ZXN_READ_REG(0x53);
    ZXN_WRITE_REG(0x53,61);
    memset((void *)(0x6000u+(uint16_t)rt.print_line*40),0,21u*40);
    rt_text_mask(5,rt.print_line,21,0,0);
    next_show_picture(0);
    ZXN_WRITE_REG(0x53,saved);
}
