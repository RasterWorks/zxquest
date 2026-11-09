#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
static uint8_t scan(void){uint8_t r;uint16_t i=0;do{r=rt_step(i?0:5);++i;}while(r==VM_RUNNING && i<500);return r;}
int main(void){
    uint8_t f,r,first=1,last=0,changes=0,moves=0;uint16_t i;rt_actor a;char directory[16];
    next_init();strcpy(rt_buffer,"build/closet-tests.txt");f=esx_f_open(rt_buffer,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    strcpy(directory,"games/sq1");r=rt_init(directory);if(!r)r=scan();if(!r){rt_key(' ',0);r=scan();}
    if(r==VM_WAIT && rt.ui==RT_UI_STRING){rt_key('R',0);rt_key('o',0);rt_key('g',0);rt_key('e',0);rt_key('r',0);rt_key(13,0);r=scan();}
    for(i=0;i<160 && !r;++i){
        rt_actor_read(0,&a);
        sprintf(rt_buffer,"%u room=%u x=%u y=%u cel=%u loop=%u dir=%u v6=%u flags=%u motion=%u control=%u\n",i,(unsigned)rt_vm.vars[0],(unsigned)a.x,(unsigned)a.y,(unsigned)a.cel,(unsigned)a.loop,(unsigned)a.direction,(unsigned)rt_vm.vars[6],(unsigned)a.flags,(unsigned)a.motion,(unsigned)rt.player_control);
        esx_f_write(f,rt_buffer,strlen(rt_buffer));
        if(rt_vm.vars[0]==2 && a.motion==RT_MOTION_MOVE){++moves;if(!first && a.cel!=last)++changes;first=0;last=a.cel;}
        if(moves && rt.player_control)break;
        r=scan();
    }
    sprintf(rt_buffer,"%s opening walk: samples=%u cel_changes=%u result=%u control=%u direction=%u\n",!r && moves>2 && changes>1 && rt.player_control && !rt_vm.vars[6]?"PASS":"FAIL",(unsigned)moves,(unsigned)changes,(unsigned)r,(unsigned)rt.player_control,(unsigned)rt_vm.vars[6]);
    esx_f_write(f,rt_buffer,strlen(rt_buffer));esx_f_close(f);for(;;){}
}
