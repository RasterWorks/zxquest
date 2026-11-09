#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "agi/picture.h"
static uint8_t file,checks,failures;
static void check(uint8_t ok,const char *s) {
    ++checks;if(!ok)++failures;sprintf(rt_buffer,"%s %s\n",ok?"PASS":"FAIL",s);
    esx_f_write(file,rt_buffer,strlen(rt_buffer));
}
static uint8_t cmd(uint8_t op,uint8_t id,uint8_t b,uint8_t c) {
    uint8_t p[7]={0};p[0]=id;p[1]=b;p[2]=c;return rt_command(&rt_vm,op,p);
}
static void reset(void) {
    rt_actor_reset(1);agi_picture_clear();next_scene_capture();rt.picture_valid=1;
    rt.horizon=36;rt.block_on=0;rt.player_control=1;rt_vm.vars[6]=0;rt.random=0x7341;
    agi_vm_set_flag(&rt_vm,200,0);
}
static void seed(uint8_t id,uint8_t x,uint8_t y,uint8_t step) {
    rt_actor a;memset(&a,0,sizeof(a));a.x=x;a.y=y;a.width=a.height=1;a.view=2;
    a.step=step;a.step_time=a.step_count=1;
    a.flags=ACT_ANIMATED|ACT_DRAWN|ACT_UPDATE|ACT_FIXED_LOOP;rt_actor_write(id,&a);
}
static void follow(void) {
    static const uint8_t positions[]={80,110,70,110,70,100,70,90,80,90,90,90,90,100,90,110};
    static const int8_t dx[]={0,1,1,1,0,-1,-1,-1},dy[]={-1,-1,0,1,1,1,0,-1};
    uint8_t i,r;rt_actor a;
    reset();seed(0,80,100,1);
    for(i=0;i<8;++i) {
        seed(255,positions[2*i],positions[2*i+1],1);agi_vm_set_flag(&rt_vm,200,1);
        r=cmd(0x53,255,3,200);rt_actor_read(255,&a);
        if(!i)check(!r&&a.motion==RT_MOTION_FOLLOW&&a.target_x==3&&a.target_y==255&&!agi_vm_flag(&rt_vm,200),"follow clears completion flag and initializes distance/detour");
        if(!r)r=rt_actor_tick();rt_actor_read(255,&a);
        check(!r&&a.direction==i+1&&a.x==positions[2*i]+dx[i]&&a.y==positions[2*i+1]+dy[i]&&!agi_vm_flag(&rt_vm,200),"follower chooses correct compass direction and moves actor 255");
    }
    reset();seed(0,80,100,1);seed(1,76,100,1);cmd(0x53,1,3,200);
    r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&a.x==77&&a.motion==RT_MOTION_FOLLOW&&!agi_vm_flag(&rt_vm,200),"distance equal to threshold is not arrival");
    rt_actor_tick();r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&a.x==78&&!a.motion&&!a.direction&&a.step==1&&agi_vm_flag(&rt_vm,200),"arrival stops follow and sets flag on next eligible tick");
    seed(1,74,100,4);cmd(0x53,1,1,200);rt_actor_read(1,&a);
    check(a.target_x==4&&a.step==4,"follow distance cannot be smaller than movement step");
    rt_actor_tick();r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&!a.motion&&a.step==4&&agi_vm_flag(&rt_vm,200),"follow completion retains actual step size");
    seed(0,70,100,1);rt_actor_read(0,&a);a.width=20;rt_actor_write(0,&a);
    seed(1,78,100,1);rt_actor_read(1,&a);a.width=4;a.flags|=ACT_IGNORE_OBJECTS;rt_actor_write(1,&a);
    cmd(0x53,1,1,200);r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&!a.motion&&agi_vm_flag(&rt_vm,200),"follow arrival compares baseline centres, not left edges");
    reset();seed(0,80,100,1);seed(1,70,100,2);rt_vm.vars[6]=3;cmd(0x53,1,2,200);
    rt_actor_tick();rt_actor_tick();cmd(0x25,0,60,90);r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&a.direction==8&&a.x==72&&a.y==98,"follower changes direction when ego moves");
}
static void blocked_and_delayed(void) {
    uint8_t r;uint16_t random;rt_actor a;
    reset();seed(0,50,80,1);seed(1,40,80,1);
    agi_pixel_write(80u*160+41,0xf0,0);next_scene_capture();cmd(0x53,1,2,200);
    r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&a.x==40&&a.y==80&&(a.ignore_blocks&ACT_DIDNT_MOVE)&&!agi_vm_flag(&rt_vm,200),"blocked follow does not pass terrain or report arrival");
    random=rt.random;r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&a.motion==RT_MOTION_FOLLOW&&a.direction>=1&&a.direction<=8&&a.target_y>=1&&a.target_y<=5&&rt.random!=random&&!agi_vm_flag(&rt_vm,200),"blocked follower chooses a bounded nonzero detour");
    agi_picture_clear();next_scene_capture();a.x=40;a.y=80;a.target_y=0;a.ignore_blocks&=~ACT_DIDNT_MOVE;rt_actor_write(1,&a);
    r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&a.x==41&&a.direction==3&&!(a.ignore_blocks&ACT_DIDNT_MOVE),"follower resumes pursuit after detour and barrier removal");
    a.step_time=a.step_count=3;rt_actor_write(1,&a);random=rt.random;
    rt_actor_tick();rt_actor_tick();rt_actor_read(1,&a);
    check(a.x==41&&a.step_count==1&&rt.random==random,"step.time delays automatic decisions and movement");
    r=rt_actor_tick();rt_actor_read(1,&a);check(!r&&a.x==42,"follow moves on the scheduled third tick");
    cmd(0x3a,1,0,0);r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&a.x==42&&a.step_count==3,"stop.update freezes follow timing");
    cmd(0x53,1,2,200);rt_actor_read(1,&a);check(a.flags&ACT_UPDATE,"follow command re-enables actor updates");
    cmd(0x24,1,0,0);random=rt.random;r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&a.x==42&&a.step_count==3&&rt.random==random,"erased follower neither moves nor consumes random numbers");
    reset();seed(0,80,100,1);seed(1,40,80,0);cmd(0x53,1,0,200);
    rt_actor_tick();r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&a.x==40&&a.y==80&&!agi_vm_flag(&rt_vm,200),"zero-step follow remains bounded while blocked");
    cmd(0x53,0,0,200);r=rt_actor_tick();rt_actor_read(0,&a);
    check(!r&&!a.motion&&!rt_vm.vars[6]&&agi_vm_flag(&rt_vm,200),"ego following itself completes without a random loop");
}
static void wander(void) {
    uint8_t r;uint16_t random;rt_actor a;
    reset();seed(255,40,80,1);cmd(0x54,255,0,0);random=rt.random;r=rt_actor_tick();rt_actor_read(255,&a);
    check(!r&&a.motion==RT_MOTION_WANDER&&a.target_x==255&&a.direction<=8&&rt.random!=random,"wander chooses direction and preserves AGI zero-count wrap");
    a.x=40;a.y=80;a.direction=3;a.target_x=10;a.ignore_blocks=0;a.step_time=a.step_count=3;rt_actor_write(255,&a);
    random=rt.random;rt_actor_tick();rt_actor_tick();rt_actor_read(255,&a);
    check(a.target_x==10&&a.x==40&&rt.random==random,"wander countdown waits for step.time");
    r=rt_actor_tick();rt_actor_read(255,&a);
    check(!r&&a.target_x==9&&a.x==41&&a.direction==3&&rt.random==random,"wander retains direction until expiry or collision");
    a.x=40;a.target_x=1;a.step_time=a.step_count=1;a.ignore_blocks=ACT_DIDNT_MOVE;rt_actor_write(255,&a);
    r=rt_actor_tick();rt_actor_read(255,&a);
    check(!r&&a.target_x>=6&&a.target_x<=50&&a.direction<=8,"blocked non-ego wanderer refreshes a short counter");
    a.x=40;a.y=80;a.direction=3;a.target_x=10;a.ignore_blocks=0;rt_actor_write(255,&a);
    agi_pixel_write(80u*160+41,0xf0,0);next_scene_capture();r=rt_actor_tick();rt_actor_read(255,&a);
    check(!r&&a.x==40&&a.y==80&&(a.ignore_blocks&ACT_DIDNT_MOVE),"wander observes the picture collision map");
    random=rt.random;r=rt_actor_tick();rt_actor_read(255,&a);
    check(!r&&rt.random!=random&&a.motion==RT_MOTION_WANDER&&!(a.x==41&&a.y==80),"collision makes wander choose a new direction");
    agi_picture_clear();next_scene_capture();a.x=159;a.y=80;a.direction=3;a.target_x=10;a.ignore_blocks=0;rt_actor_write(255,&a);
    r=rt_actor_tick();rt_actor_read(255,&a);
    check(!r&&a.x==159&&a.motion==RT_MOTION_WANDER&&rt_vm.vars[4]==255&&rt_vm.vars[5]==2&&!agi_vm_flag(&rt_vm,200),"wander edge emits event without completing a move.obj");
    a.x=60;a.y=100;a.direction=4;a.target_x=200;a.ignore_blocks=0;rt_actor_write(255,&a);
    r=rt_actor_tick();rt_actor_read(255,&a);
    check(!r&&a.x==61&&a.y==101&&a.direction==4&&!(a.ignore_blocks&ACT_DIDNT_MOVE),"unobstructed wandering diagonal advances both axes");
    a.x=60;a.y=167;a.direction=4;a.target_x=200;rt_actor_write(255,&a);
    r=rt_actor_tick();rt_actor_read(255,&a);
    check(!r&&a.x==61&&a.y==167&&(a.ignore_blocks&ACT_DIDNT_MOVE)&&rt_vm.vars[5]==3,"bottom-clipped wander diagonal requests a new direction");
    random=rt.random;rt_actor_tick();check(rt.random!=random,"clipped wander rerolls before its long countdown expires");
    a.x=60;a.y=37;a.direction=2;a.target_x=200;a.ignore_blocks=0;rt_actor_write(255,&a);
    r=rt_actor_tick();rt_actor_read(255,&a);
    check(!r&&a.x==61&&a.y==37&&(a.ignore_blocks&ACT_DIDNT_MOVE)&&rt_vm.vars[5]==1,"horizon-clipped wander diagonal requests a new direction");
    a.x=159;a.y=100;a.direction=4;a.target_x=200;a.ignore_blocks=0;rt_actor_write(255,&a);
    r=rt_actor_tick();rt_actor_read(255,&a);
    check(!r&&a.x==159&&a.y==101&&(a.ignore_blocks&ACT_DIDNT_MOVE)&&rt_vm.vars[5]==2,"side-clipped wander diagonal requests a new direction");
    seed(0,60,100,1);r=cmd(0x54,0,0,0);if(!r)r=rt_actor_tick();rt_actor_read(0,&a);
    check(!r&&!rt.player_control&&rt_vm.vars[6]==a.direction,"ego wander takes program control and maintains v6");
    r=cmd(0x84,255,0,0);rt_actor_read(0,&a);
    check(!r&&rt.player_control&&!a.motion,"player.control cancels ego automatic motion regardless of unused operand");
    rt_actor_read(255,&a);check(a.motion==RT_MOTION_WANDER,"player.control leaves other actors wandering");
    rt_vm.vars[6]=0;rt_key(0,0x4d);r=rt_actor_tick();rt_actor_read(0,&a);
    check(!r&&a.direction==3&&rt_vm.vars[6]==3,"keyboard direction works after returning player control");
}
static void controls_and_vm(void) {
    uint8_t r;uint16_t n=0;rt_actor a;
    reset();seed(0,80,100,1);seed(1,40,80,1);seed(2,100,120,1);
    r=agi_vm_start(&rt_vm,12);while(r==VM_RUNNING&&n++<20)r=agi_vm_run(&rt_vm,10);
    rt_actor_read(1,&a);
    check(!r&&rt_vm.vars[205]==99&&a.motion==RT_MOTION_FOLLOW&&!agi_vm_flag(&rt_vm,210),"LOGIC follow/wander commands dispatch and continue normally");
    rt_actor_read(2,&a);check(a.motion==RT_MOTION_WANDER,"script command starts wander on its specified actor");
    rt_actor_tick();rt_actor_read(1,&a);r=cmd(0x55,1,0,0);rt_actor_read(1,&a);
    check(!r&&!a.motion&&a.direction==4&&!agi_vm_flag(&rt_vm,210),"normal.motion cancels follow without raising completion flag");
    cmd(0x53,1,2,200);r=cmd(0x4d,1,0,0);rt_actor_read(1,&a);
    check(!r&&!a.motion&&!a.direction&&!agi_vm_flag(&rt_vm,200),"stop.motion cancels automatic movement and direction");
    cmd(0x54,1,0,0);r=cmd(0x4e,1,0,0);rt_actor_read(1,&a);
    check(!r&&!a.motion,"start.motion returns a wanderer to normal movement");
    rt_actor_reset(0);rt_actor_read(2,&a);
    check(!a.motion&&!a.direction&&!a.ignore_blocks&&sizeof(a)==32,"room reset clears automatic motion without enlarging actor records");
}
static uint8_t droid_script(void) {
    uint8_t r=agi_vm_start(&rt_vm,110);uint16_t n=0;
    while(r==VM_RUNNING&&n++<100)r=agi_vm_run(&rt_vm,100);
    return r;
}
static void original_droid(void) {
    char path[16];uint8_t r,x;rt_actor a;
    /* Stage the original droid module's documented script state. This is
       an integration fixture, not a claim to have played to the planet. */
    strcpy(path,"games/sq1");r=rt_init(path);if(!r){reset();seed(0,100,100,1);r=cmd(0x29,0,1,0);}
    rt_vm.vars[0]=18;rt_vm.vars[1]=15;if(!r)r=droid_script();
    check(!r,"original SQ1 droid module initializes its views and sounds");if(r)return;
    agi_vm_set_flag(&rt_vm,5,0);agi_vm_set_flag(&rt_vm,97,1);agi_vm_set_flag(&rt_vm,191,1);
    rt_vm.vars[235]=2;rt_vm.vars[98]=40;rt_vm.vars[99]=100;r=droid_script();rt_actor_read(16,&a);
    check(!r&&a.view==46&&a.motion==RT_MOTION_FOLLOW&&a.target_x==10&&!agi_vm_flag(&rt_vm,235),"original LOGIC 110 spawns the droid and starts follow.ego");if(r)return;
    x=a.x;r=rt_actor_tick();if(!r)r=rt_actor_render();rt_actor_read(16,&a);
    check(!r&&a.x==x+1&&a.y==100&&a.direction==3,"original droid cel moves toward Roger and composites successfully");
    agi_vm_set_flag(&rt_vm,102,0);agi_vm_set_flag(&rt_vm,92,1);rt_vm.vars[235]=2;
    r=droid_script();rt_actor_read(16,&a);
    check(!r&&a.motion==RT_MOTION_WANDER&&a.x==59&&a.y==73,"original droid branch switches to wander while Roger is hidden");
    if(!r)r=rt_actor_tick();if(!r)r=rt_actor_render();
    check(!r,"original droid wander updates and renders without unsupported commands");
}
uint8_t bank_follow_tests(void) {
    char path[40];uint8_t r,m2=ZXN_READ_REG(0x52),m3=ZXN_READ_REG(0x53),m4=ZXN_READ_REG(0x54);
    checks=failures=0;strcpy(path,"build/follow-tests.txt");file=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    strcpy(path,"build/runtime-fixtures");r=rt_init(path);check(!r,"automatic-motion fixtures initialize");
    if(!r){follow();blocked_and_delayed();wander();controls_and_vm();original_droid();}
    /* MMU2 caches the last accessed picture page by design. Other windows
       must return to their caller's banks across nested motion callbacks. */
    (void)m2;check(ZXN_READ_REG(0x53)==m3&&ZXN_READ_REG(0x54)==m4,"automatic motion preserves shared data/code bank mappings");
    sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",(unsigned)checks,(unsigned)failures);
    esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_close(file);return !failures;
}
