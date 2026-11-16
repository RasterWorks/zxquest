#include <stdio.h>
#include <string.h>
#include <arch/zxn/esxdos.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "next/pacing.inc"
static uint8_t file,checks,failures;
static void check(uint8_t yes,const char *name){++checks;if(!yes)++failures;sprintf(rt_buffer,"%s %s\n",yes?"PASS":"FAIL",name);esx_f_write(file,rt_buffer,strlen(rt_buffer));}
static uint16_t measured(uint8_t speed){
    cycle_pacer pace={0,0,0,0};uint16_t last,now,began,before,goal;uint8_t r;
    strcpy(rt_buffer,"build/runtime-fixtures");r=rt_init(rt_buffer);
    /* Run the authored initialization before measuring steady gameplay. */
    while(!r && !rt.cycles){r=rt_step(10);if(r==VM_RUNNING)r=0;}
    check(!r,"fixture initializes for speed measurement");
    rt_vm.vars[10]=speed;goal=rt.cycles+8;last=began=next_clock();
    while(!r && rt.cycles<goal && (uint16_t)(next_clock()-began)<1000){
        now=next_clock();if(rt_vm.status==VM_DONE && !pace_ready(&pace,now,speed))continue;
        if(rt_vm.status!=VM_RUNNING)pace.began=now;
        before=rt.cycles;r=rt_step(now-last);last=now;
        if(rt.cycles!=before)pace_end(&pace,next_clock(),speed);
        if(r==VM_RUNNING)r=0;
    }
    now=next_clock()-began;check(!r && rt.cycles==goal,"eight complete logic/actor cycles without skipping");
    sprintf(rt_buffer,"SPEED %u eight cycles=%u frames\n",speed,now);esx_f_write(file,rt_buffer,strlen(rt_buffer));return now;
}
int main(void){
    cycle_pacer p={65530,0,0,0};uint16_t slow,normal,fast,fastest;
    next_init();next_clock_start();strcpy(rt_buffer,"build/speed-tests.txt");file=esx_f_open(rt_buffer,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    pace_end(&p,4,2);check(p.wait==1,"normal accounts for clock wrap and retains its minimum idle frame");
    p.began=100;pace_end(&p,140,3);check(p.wait==10,"slow adds idle time even after expensive frame");
    check(!pace_ready(&p,149,3) && pace_ready(&p,150,3),"slow deadline releases at the correct frame");
    check(pace_ready(&p,141,0),"changing to fastest cancels a pending slow delay");
    slow=measured(3);normal=measured(2);fast=measured(1);fastest=measured(0);
    check(slow>normal && normal>fast && fast>fastest,"measured speed settings are strictly ordered");
    sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",checks,failures);esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_close(file);for(;;){}
}
