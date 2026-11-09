/* Cold validation/conversion in overlay 56, never called by the IRQ. */
#include <arch/zxn.h>
#include <intrinsic.h>
#include <z80.h>
#include <string.h>
#include "runtime.h"
#include "sound.h"
static const uint8_t volume[16]={15,14,14,13,12,12,11,10,10,9,8,8,7,6,6,0};
void rt_sound_poll(void) {
    uint8_t done,iff=z80_get_int_state();intrinsic_di();
    done=sound_done;sound_done=0;z80_set_int_state(iff);
    if(done && rt.sound_active) {
        agi_vm_set_flag(&rt_vm,rt.sound_flag,1);rt.sound_active=0;
    }
}
void rt_sound_stop(void) { next_sound_halt();rt_sound_poll();rt.sound_active=0; }
static uint8_t prepare(resource_handle h,uint16_t size,uint16_t *offsets) {
    uint16_t at,n,d,ch,end[4],j,period;uint32_t total,longest=0;
    uint8_t saved,r=VM_HEADER,*p;
    if(size<8 || size>SOUND_CAPACITY)return VM_HEADER;
    /* Resident staging: resource_copy temporarily maps the cache into MMU3. */
    saved=ZXN_READ_REG(0x53);
    for(at=0;at<size;at+=n) {
        n=size-at;if(n>sizeof(rt_buffer))n=sizeof(rt_buffer);
        if(resource_copy(h,at,rt_buffer,n)){r=VM_IO;goto leave;}
        ZXN_WRITE_REG(0x53,SOUND_PAGE);memcpy((void *)(SOUND_BASE+at),rt_buffer,n);
    }
    p=(uint8_t *)SOUND_BASE;
    for(ch=0;ch<4;++ch) {
        at=p[ch*2]|((uint16_t)p[ch*2+1]<<8);offsets[ch]=at;total=0;
        if(at<8 || at>size-2)goto leave;
        for(;;) {
            if(at>size-2)goto leave;
            d=p[at]|((uint16_t)p[at+1]<<8);
            if(d==65535)break;
            /* V2 zero notes can stall original players. Reject explicitly. */
            if(!d || size-at<5)goto leave;
            total+=d;at+=5;
        }
        end[ch]=at+2;if(total>longest)longest=total;
    }
    /* Disjoint streams prevent double conversion or reads of another header. */
    for(ch=0;ch<4;++ch)for(j=0;j<ch;++j)
        if(offsets[ch]<end[j] && offsets[j]<end[ch])goto leave;
    for(ch=0;ch<4;++ch)for(at=offsets[ch];at<end[ch]-2;at+=5) {
        d=((uint16_t)(p[at+2]&63)<<4)|(p[at+3]&15);
        if(ch==3){p[at+2]=p[at+3]&7;p[at+3]=0;}
        else {
            /* 1.75 MHz Next AY /16 vs 3.579545 MHz SN /32, rounded. */
            period=((uint32_t)d*35000UL+17897UL)/35795UL;
            if(d && !period)period=1;
            p[at+2]=period;p[at+3]=period>>8;
        }
        p[at+4]=(ch!=3 && !d)?0:volume[p[at+4]&15];
    }
    rt.sound_ticks=longest;r=VM_DONE;
leave:
    ZXN_WRITE_REG(0x53,saved);return r;
}
uint8_t rt_sound_start(uint8_t id,uint8_t flag) {
    resource_handle h;uint16_t size,offsets[4];uint8_t r;
    rt_sound_stop();rt.sound_flag=flag;rt.sound_ticks=0;
    if(!agi_vm_flag(&rt_vm,9)){agi_vm_set_flag(&rt_vm,flag,1);return VM_DONE;}
    r=resource_acquire(RESOURCE_SOUND,id,&h);if(r)return VM_IO;
    r=resource_size(h,&size);if(!r)r=prepare(h,size,offsets);
    resource_release(h);if(r)return r;
    agi_vm_set_flag(&rt_vm,flag,0);rt.sound_active=1;
    next_sound_begin(offsets);rt_sound_poll();return VM_DONE;
}
