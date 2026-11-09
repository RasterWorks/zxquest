#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/save.h"
static uint8_t file,failures;
extern void rt_host_key(uint8_t);
static void log_state(const char *game,uint16_t cycle,uint8_t r) {
    sprintf(rt_buffer,"%s cycle=%u result=%u room=%u ui=%u control=%u logic=%u pc=%u opcode=%02X test=%u\n",game,cycle,(unsigned)r,(unsigned)rt_vm.vars[0],(unsigned)rt.ui,(unsigned)rt.player_control,(unsigned)rt_vm.logic,rt_vm.pc,(unsigned)rt_vm.opcode,(unsigned)rt_vm.is_test);
    esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_sync(file);
}
static void screen(const char *game) {
    uint8_t f,page,saved=ZXN_READ_REG(0x53);uint16_t at;char path[64];sprintf(path,"build/%s-startup.l2",game);
    f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    for(page=40;page<50;++page)for(at=0;at<8192;at+=512){ZXN_WRITE_REG(0x53,page);memcpy(rt_buffer,(void *)(0x6000u+at),512);esx_f_write(f,rt_buffer,512);}
    esx_f_close(f);ZXN_WRITE_REG(0x53,saved);
}
static uint8_t scan(void) {uint8_t r;uint16_t n=0;do{r=rt_step(n?0:5);++n;}while(r==VM_RUNNING&&n<500);return r;}
static void sq1_screens(void) {
    uint8_t r,i;
    /* Enter the original pod-monitor instruction path, staging its normal
       script flags/counter in RAM rather than replaying the full adventure. */
    agi_vm_stop(&rt_vm);agi_vm_set_flag(&rt_vm,5,0);agi_vm_set_flag(&rt_vm,32,1);
    agi_vm_set_flag(&rt_vm,41,1);rt_vm.vars[35]=1;rt_vm.vars[0]=10;
    rt_vm.scan[10]=746;r=agi_vm_start(&rt_vm,10);
    for(i=0;i<4 && r==VM_RUNNING;++i)r=rt_step(0);
    sprintf(rt_buffer,"sq1 pod wait=%u gfx=%u pc=%u\n",(unsigned)r,(unsigned)rt.gfx,rt_vm.pc);esx_f_write(file,rt_buffer,strlen(rt_buffer));
    if(r!=VM_RUNNING || rt.gfx)++failures;
    rt_key('q',0);
    for(i=0;i<20 && r==VM_RUNNING;++i)r=rt_step(0);
    sprintf(rt_buffer,"sq1 pod key=%u gfx=%u input=%u\n",(unsigned)r,(unsigned)rt.gfx,(unsigned)rt.input_enabled);esx_f_write(file,rt_buffer,strlen(rt_buffer));
    if(r || !rt.gfx || !rt.input_enabled || rt.key_pending)++failures;
    /* Execute the exact ship-countdown tail reported by the user, through
       shake.screen and the original new.room(77) / explosion dialogue. */
    agi_vm_stop(&rt_vm);rt_vm.vars[0]=2;rt_vm.scan[0]=1324;r=scan();
    log_state("sq1 explosion",0,r);
    if(r!=VM_WAIT || rt_vm.vars[0]!=77 || rt.ui!=RT_UI_MESSAGE)++failures;
}
static void game(const char *name) {
    char path[40],id[4];uint8_t r,old=255,saved_room,x,y,before_failures=failures;uint16_t cycle;rt_actor a;
    strcpy(id,name);sprintf(path,"games/%s",id);r=rt_init(path);log_state(id,0,r);if(r){++failures;return;}
    for(cycle=0;cycle<3000;++cycle) {
        r=scan();
        if(rt_vm.vars[0]!=old || r!=VM_DONE){log_state(id,cycle,r);old=rt_vm.vars[0];}
        if(r==VM_WAIT) {
            if(rt.ui==RT_UI_STRING){rt_key('A',0);rt_key('l',0);}
            rt_key(13,0);continue;
        }
        if(r!=VM_DONE){++failures;break;}
        if(cycle>0 && rt.player_control && rt.input_enabled && rt.picture_visible)break;
        if(cycle%10==0)rt_key(' ',0);
    }
    log_state(id,cycle,r);screen(id);
    if(!r && cycle<3000) {
        saved_room=rt_vm.vars[0];r=rt_save_slot(9,0);rt_vm.vars[200]=199;
        if(!r)r=rt_save_slot(9,1);
        sprintf(rt_buffer,"%s save/restore=%u restored_room=%u expected=%u\n",id,(unsigned)r,(unsigned)rt_vm.vars[0],(unsigned)saved_room);esx_f_write(file,rt_buffer,strlen(rt_buffer));
        if(r || rt_vm.vars[0]!=saved_room)++failures;
        if(!r){r=scan();log_state(id,cycle+1,r);if(r && r!=VM_WAIT)++failures;}
        /* Continue twenty cycles to exercise the restored room and parser. */
        for(cycle=0;cycle<20 && !r;++cycle)r=scan();
        log_state(id,cycle,r);if(r && r!=VM_WAIT)++failures;
        if(r==VM_WAIT){rt_key(13,0);r=scan();}
        rt_actor_read(0,&a);x=a.x;y=a.y;rt_key(0,0x4d);
        for(cycle=0;cycle<8 && !r;++cycle)r=scan();rt_key(0,0x4d);rt_actor_read(0,&a);
        sprintf(rt_buffer,"%s movement=%u result=%u ui=%u from=%u,%u to=%u,%u\n",id,(unsigned)(x!=a.x || y!=a.y),(unsigned)r,(unsigned)rt.ui,(unsigned)x,(unsigned)y,(unsigned)a.x,(unsigned)a.y);esx_f_write(file,rt_buffer,strlen(rt_buffer));
        if(r==VM_WAIT && rt.ui==RT_UI_MESSAGE){rt_key(13,0);r=scan();}
        if(r || (x==a.x && y==a.y))++failures;
        rt_key('l',0);rt_key('o',0);rt_key('o',0);rt_key('k',0);rt_key(13,0);r=scan();
        sprintf(rt_buffer,"%s LOOK result=%u ui=%u\n",id,(unsigned)r,(unsigned)rt.ui);esx_f_write(file,rt_buffer,strlen(rt_buffer));
        if(r==VM_WAIT){rt_key(13,0);r=scan();}if(r)++failures;
        rt_host_key(132);r=scan();
        sprintf(rt_buffer,"%s F5 result=%u ui=%u\n",id,(unsigned)r,(unsigned)rt.ui);esx_f_write(file,rt_buffer,strlen(rt_buffer));
        if(r==VM_WAIT && rt.ui==RT_UI_SAVE) {
            strcpy(path,id);strcat(path,"-save");screen(path);
            rt_key('8',0);rt_key(13,0);rt_key(13,0);rt_key(13,0);
            if(rt.ui || rt.error)++failures;
            r=scan();
            rt_host_key(134);r=scan();
            sprintf(rt_buffer,"%s F7 result=%u ui=%u\n",id,(unsigned)r,(unsigned)rt.ui);esx_f_write(file,rt_buffer,strlen(rt_buffer));
            if(r==VM_WAIT && rt.ui==RT_UI_SAVE){rt_key('8',0);rt_key(13,0);rt_key(13,0);r=scan();}
            else ++failures;
            log_state(id,3001,r);
            if(r && r!=VM_WAIT)++failures;
        } else ++failures;
    } else if(cycle==3000)++failures;
    sprintf(rt_buffer,"%s failures=%u\n",id,(unsigned)(failures-before_failures));esx_f_write(file,rt_buffer,strlen(rt_buffer));
}
uint8_t bank_compatibility_tests(void) {
    char path[48];strcpy(path,"build/compatibility-tests.txt");file=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);failures=0;
    game("kq1");game("pq1");game("sq1");sq1_screens();
    sprintf(rt_buffer,"SUMMARY failures=%u\n",(unsigned)failures);esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_close(file);return !failures;
}
