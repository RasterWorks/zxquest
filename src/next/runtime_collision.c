/* AGI baseline collision and placement rules adapted from Sarien checks.c.
 * Copyright (C) 1999-2001 Stuart George and Claudio Matsuoka
 * GNU General Public License; see COPYING. Banked map access is new.
 */
#include <arch/zxn.h>
#include "runtime.h"
#include "agi/view.h"

/* Tick-start baselines live above the renderer scratch in data page 91.
   Capture all of them before moving any actor, so ID order cannot hide a
   crossing when both actors move during the same tick. */
void rt_motion_begin(void) {
    uint16_t id; rt_actor a; uint8_t saved=ZXN_READ_REG(0x53);
    ZXN_WRITE_REG(0x53,91);
    for(id=0;id<256;++id) {
        if(!rt.occupied[id>>3]){id|=7;continue;}
        if(!(rt.occupied[id>>3]&(1u<<(id&7))))continue;
        rt_actor_read(id,&a);((uint8_t *)0x6500)[id]=a.y;
    }
    ZXN_WRITE_REG(0x53,saved);
    rt_vm.vars[2]=rt_vm.vars[4]=rt_vm.vars[5]=0;
}
static uint8_t collision(uint8_t id,const rt_actor *a,int16_t x,int16_t y,uint8_t old_y) {
    uint16_t i;rt_actor other;uint8_t old_other,saved;
    if(a->flags&ACT_IGNORE_OBJECTS)return 0;
    for(i=0;i<256;++i) {
        if(!rt.occupied[i>>3]){i|=7;continue;}
        if(i==id || !(rt.occupied[i>>3]&(1u<<(i&7))))continue;
        rt_actor_read(i,&other);
        if((other.flags&(ACT_ANIMATED|ACT_DRAWN|ACT_IGNORE_OBJECTS))!=(ACT_ANIMATED|ACT_DRAWN))continue;
        /* AGI includes the touching endpoint, not just overlapping pixels. */
        if(x+a->width<other.x || x>(uint16_t)other.x+other.width)continue;
        if(y==other.y)return 1;
        if(y==old_y)continue; /* Placement checks have no swept baseline. */
        saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,91);
        old_other=((uint8_t *)0x6500)[i];ZXN_WRITE_REG(0x53,saved);
        if((y>other.y && old_y<old_other) || (y<other.y && old_y>old_other))return 1;
    }
    return 0;
}
static uint8_t terrain(uint8_t id,const rt_actor *a,uint8_t x,uint8_t y) {
    uint8_t i,p,page=255,saved,water=0,trigger=0,pass=1;
    uint16_t at;
    if(!rt.picture_valid || ((a->flags&ACT_FIXED_PRIORITY) && a->priority==15))goto flags;
    water=1;at=(uint16_t)y*160+x;saved=ZXN_READ_REG(0x53);
    for(i=0;i<a->width;++i,++at) {
        /* Always read the clean picture: composited sprite priorities must
           never erase a control line or manufacture land over water. */
        if((at>>13)!=page){page=at>>13;ZXN_WRITE_REG(0x53,36+page);}
        p=*((uint8_t *)(0x6000u+(at&8191)))>>4;
        if(!p){pass=0;break;}
        if(p==3)continue;
        water=0;
        if(p==1 && !(a->ignore_blocks&ACT_IGNORE_BLOCKS)){pass=0;break;}
        if(p==2)trigger=1;
    }
    ZXN_WRITE_REG(0x53,saved);
    if(pass && ((!water && (a->ignore_blocks&ACT_ON_WATER)) ||
                (water && (a->ignore_blocks&ACT_ON_LAND))))pass=0;
flags:
    if(!id){agi_vm_set_flag(&rt_vm,0,water);agi_vm_set_flag(&rt_vm,3,trigger);}
    return pass;
}
uint8_t rt_actor_valid(uint8_t id,const rt_actor *a,int16_t x,int16_t y,uint8_t old_y) {
    if(!a->width || !a->height || x<0 || x+a->width>160 ||
       y+1<a->height || y>=168 || (!(a->flags&ACT_IGNORE_HORIZON) && y<=rt.horizon))return 0;
    return !collision(id,a,x,y,old_y) && terrain(id,a,x,y);
}
uint8_t rt_actor_fix(uint8_t id,rt_actor *a) {
    int16_t x=a->x,y=a->y;uint16_t size,count;uint8_t dir;
    if(!a->width || a->width>160 || !a->height || a->height>168)return VM_ARGUMENT;
    if(!(a->flags&ACT_IGNORE_HORIZON) && y<=rt.horizon)y=(uint16_t)rt.horizon+1;
    /* Sierra's west/south/east/north expanding spiral, with signed positions
       and a finite search even for a corrupt/all-blocked picture. */
    dir=0;size=count=1;
    while(!rt_actor_valid(id,a,x,y,y)) {
        if(dir==0)--x;else if(dir==1)++y;else if(dir==2)++x;else --y;
        if(!--count){dir=(dir+1)&3;if(!(dir&1))++size;count=size;}
        if(size>512)return VM_ARGUMENT;
    }
    a->x=x;a->y=y;return VM_DONE;
}
