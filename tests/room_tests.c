#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
static uint8_t file,checks,failures;
static void log(const char *s){esx_f_write(file,(void *)s,strlen(s));}
static void check(uint8_t ok,const char *s){++checks;if(!ok)++failures;sprintf(rt_buffer,"%s %s\n",ok?"PASS":"FAIL",s);log(rt_buffer);}
static uint8_t scan(void) {
    uint8_t r;uint16_t n=0;
    do{r=rt_step(n?0:5);++n;}while(r==VM_RUNNING && n<500);
    return n==500?VM_ARGUMENT:r;
}
static uint8_t advance(void) {
    uint8_t r=scan(),count=0;
    while(r==VM_WAIT && rt.ui==RT_UI_MESSAGE && count++<8){rt_key(13,0);r=scan();}
    return r;
}
static void sentence(const char *s){while(*s)rt_key(*s++,0);rt_key(13,0);}
static void screen(const char *name) {
    char path[40];uint8_t f,page,saved=ZXN_READ_REG(0x53);strcpy(path,name);
    f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    for(page=40;page<50;++page){ZXN_WRITE_REG(0x53,page);esx_f_write(f,(void *)0x6000,8192);}
    esx_f_close(f);ZXN_WRITE_REG(0x53,saved);
}
static uint8_t walk(uint8_t direction,uint8_t target_x,uint8_t target_y,uint8_t target_room,uint16_t limit) {
    rt_actor a;uint8_t r=0;uint16_t i;rt_vm.vars[6]=direction;
    for(i=0;i<limit && !r;++i) {
        r=advance();rt_actor_read(0,&a);
        if(rt_vm.vars[0]==target_room && (target_x==255 || (a.x==target_x && a.y==target_y)))break;
    }
    rt_vm.vars[6]=0;
    sprintf(rt_buffer,"WALK dir=%u scans=%u r=%u room=%u xy=%u,%u edge=%u logic=%u pc=%u op=%02x\n",(unsigned)direction,i,(unsigned)r,(unsigned)rt_vm.vars[0],(unsigned)a.x,(unsigned)a.y,(unsigned)rt_vm.vars[2],(unsigned)rt_vm.logic,rt_vm.pc,(unsigned)rt_vm.opcode);log(rt_buffer);
    return r?r:i==limit?VM_ARGUMENT:VM_DONE;
}
static void lifecycle(void) {
    char path[40];rt_actor a;resource_cache_info info;uint8_t edge,r;uint16_t n;
    for(edge=1;edge<=4;++edge) {
        strcpy(path,"build/room-fixtures");r=rt_init(path);
        memset(&a,0,sizeof(a));a.x=100;a.y=80;a.width=7;a.height=16;a.view=1;
        a.step=5;a.step_time=3;a.cycle_time=7;a.ignore_blocks=1;a.flags=ACT_DRAWN|ACT_ANIMATED;
        rt_actor_write(0,&a);rt_vm.vars[0]=6;rt_vm.vars[2]=edge;rt_vm.vars[205]=7;
        rt_vm.scan[99]=123;rt.horizon=80;rt.block_on=1;rt.picture_valid=rt.picture_visible=1;
        if(!r)r=agi_vm_start(&rt_vm,edge&1?1:2);if(r==VM_RUNNING)r=rt_step(0);
        rt_actor_read(0,&a);
        check(r==VM_RUNNING&&rt_vm.vars[0]==7&&rt_vm.vars[1]==6&&rt_vm.vars[16]==1&&agi_vm_flag(&rt_vm,5)&&!rt_vm.vars[2],"new.room/new.room.v restart LOGIC 0 with previous room and ego view");
        check(a.x==(edge==2?0:edge==4?153:100)&&a.y==(edge==1?167:edge==3?37:80),"room entry places ego on opposite edge");
        check(a.step==1&&a.step_time==1&&a.cycle_time==1&&a.flags==ACT_UPDATE&&!a.ignore_blocks&&!rt.block_on&&rt.horizon==36&&!rt.picture_valid&&!rt_vm.scan[99],"room change resets motion timing scan starts blocks and picture validity");
        n=0;while(r==VM_RUNNING&&n++<20)r=rt_step(0);
        resource_cache_status(&info);
        check(!r&&rt_vm.vars[200]==a.x&&rt_vm.vars[201]==a.y&&rt_vm.vars[202]==6&&rt_vm.vars[203]==1&&rt_vm.vars[204]==1&&!agi_vm_flag(&rt_vm,5)&&!info.pinned,"destination script observes entry state and releases resource pins");
        r=scan();check(!r&&rt_vm.vars[204]==1,"first-scan initialization runs once per new room");
    }
}
uint8_t bank_room_tests(void) {
    char path[40];uint8_t r;uint16_t i;rt_actor a;
    checks=failures=0;strcpy(path,"build/room-tests.txt");file=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    lifecycle();
    strcpy(path,"games/sq1");r=rt_init(path);check(!r,"original SQ1 initializes for room traversal");if(r)goto done;
    r=scan();if(r)goto done;rt_key(' ',0);r=scan();
    check(r==VM_WAIT&&rt.ui==RT_UI_STRING,"original name prompt reached");if(rt.ui!=RT_UI_STRING)goto done;
    sentence("Roger");r=scan();for(i=0;i<100&&!r&&!rt.player_control;++i)r=advance();
    rt_actor_read(0,&a);check(!r&&rt_vm.vars[0]==2&&rt.player_control&&a.x==97&&a.y==66,"scripted closet exit completes with collisions enabled");if(r)goto done;
    screen("build/collision-room2.l2");
    r=walk(3,255,0,3,180);check(!r&&rt_vm.vars[1]==2,"walk east through original room 2 exit into room 3");if(r)goto done;
    screen("build/collision-room3.l2");sentence("look");r=scan();
    check(r==VM_WAIT&&rt.ui==RT_UI_MESSAGE,"adjoining room accepts original parser command");if(r==VM_WAIT){rt_key(13,0);r=advance();}if(r)goto done;
    r=walk(7,255,0,2,180);check(!r&&rt_vm.vars[1]==3,"walk west back to room 2");if(r)goto done;
    r=walk(7,255,0,1,240);check(!r&&rt_vm.vars[1]==2,"walk through automatic archive door into room 1");if(r)goto done;
    screen("build/collision-room1.l2");
    r=walk(3,255,0,2,240);check(!r&&rt_vm.vars[1]==1,"return from archive to room 2");
done:
    sprintf(rt_buffer,"STOP r=%u room=%u logic=%u pc=%u op=%02x test=%u ui=%u\n",(unsigned)r,(unsigned)rt_vm.vars[0],(unsigned)rt_vm.logic,rt_vm.pc,(unsigned)rt_vm.opcode,(unsigned)rt_vm.is_test,(unsigned)rt.ui);log(rt_buffer);
    if(r)check(0,"traversal completes without interpreter error or timeout");
    sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",(unsigned)checks,(unsigned)failures);log(rt_buffer);esx_f_close(file);return !failures;
}
