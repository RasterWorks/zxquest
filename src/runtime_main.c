#include <stdio.h>
#include "next/platform.h"
#include "next/runtime.h"
#include "next/sound.h"
#include "next/save.h"
#include <z80.h>
#include "next/pacing.inc"
extern void next_exit(void);
extern void rt_host_key(uint8_t key);
extern void rt_stop_message(uint8_t result);
int main(void) {
    uint8_t result,key,modal; uint16_t now,last,cycles;
    cycle_pacer pace={0,0,0,0};
    next_init(); result=rt_startup();
    if(result) { sprintf(rt_buffer,"STARTUP ERROR %u",(unsigned)result); next_text(0,5,rt_buffer); for(;;){} }
    /* Place loading text inside the area replaced by the first show.pic. */
    next_clock_start(); last=next_clock();
    for(;;) {
        key=next_key();
        if(key) {
            modal=rt.ui;rt_host_key(key);
            /* Disk IO on a modal key must not advance the restored clock. */
            if(modal){last=next_clock();pace.wait=0;}
        }
        if(rt_joystick_mode){modal=rt.ui;rt_joystick(z80_inp(rt_joystick_mode==1?0x1f:0x37));if(modal)last=next_clock();}
        now=next_clock();
        if(!rt.ui && !rt_control_action && rt_vm.status==VM_DONE && !pace_ready(&pace,now,rt_vm.vars[10]))continue;
        if(rt_vm.status!=VM_RUNNING && rt_vm.status!=VM_WAIT)pace.began=now;
        cycles=rt.cycles;result=rt_step(now-last);last=now;
        if(rt.cycles!=cycles)pace_end(&pace,next_clock(),rt_vm.vars[10]);
        if(rt_control_action==1){rt_control_apply();next_exit();}
        if(result!=VM_DONE && result!=VM_RUNNING && result!=VM_WAIT) {
            rt_stop_message(result);
            for(;;) {}
        }
    }
}
