#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "agi/picture.h"
static uint8_t file,checks,failures;
static void check(uint8_t ok,const char *name) {
    ++checks;if(!ok)++failures;sprintf(rt_buffer,"%s %s\n",ok?"PASS":"FAIL",name);
    esx_f_write(file,rt_buffer,strlen(rt_buffer));
}
static uint8_t cmd(uint8_t op,uint8_t id,uint8_t b,uint8_t c) {
    uint8_t p[7]={0};p[0]=id;p[1]=b;p[2]=c;return rt_command(&rt_vm,op,p);
}
static void reset(void) {
    rt_actor_reset(1);agi_picture_clear();next_scene_capture();rt.picture_valid=1;
    rt.horizon=36;rt.block_on=0;rt.player_control=1;rt_vm.vars[6]=0;
}
static void seed(uint8_t id,uint8_t x,uint8_t y,uint8_t direction,uint8_t step) {
    rt_actor a;memset(&a,0,sizeof(a));a.x=x;a.y=y;a.width=a.height=1;a.view=2;
    a.step=step;a.step_time=a.step_count=1;a.direction=direction;
    a.flags=ACT_ANIMATED|ACT_DRAWN|ACT_UPDATE|ACT_FIXED_LOOP;
    rt_actor_write(id,&a);if(!id)rt_vm.vars[6]=direction;
}
static void pixel(uint8_t x,uint8_t y,uint8_t priority) {
    agi_pixel_write((uint16_t)y*160+x,0xf0,priority<<4);next_scene_capture();
}
static uint8_t at(uint8_t id,uint8_t x,uint8_t y) {
    rt_actor a;rt_actor_read(id,&a);return a.x==x && a.y==y;
}
static void terrain(void) {
    uint8_t r;rt_actor a;
    reset();seed(0,40,80,3,1);pixel(41,80,0);r=rt_actor_tick();
    check(!r&&at(0,40,80),"black baseline pixel blocks movement");
    cmd(0x58,0,0,0);r=rt_actor_tick();check(!r&&at(0,40,80),"ignore.blocks still observes black barriers");
    pixel(41,80,1);cmd(0x59,0,0,0);r=rt_actor_tick();check(!r&&at(0,40,80),"blue baseline pixel blocks movement");
    cmd(0x58,0,0,0);r=rt_actor_tick();check(!r&&at(0,41,80),"ignore.blocks crosses blue barriers");
    reset();seed(0,40,80,3,1);rt_actor_read(0,&a);a.width=3;rt_actor_write(0,&a);pixel(43,80,0);
    r=rt_actor_tick();check(!r&&at(0,40,80),"entire foot width is tested including rightmost pixel");
    reset();seed(0,40,80,3,1);pixel(41,79,0);r=rt_actor_tick();
    check(!r&&at(0,41,80),"pixels above feet do not block movement");
    reset();seed(0,40,80,0,1);pixel(40,80,2);r=rt_actor_tick();
    check(!r&&agi_vm_flag(&rt_vm,3)&&!agi_vm_flag(&rt_vm,0),"stationary ego refreshes trigger flag 3");
    rt_vm.vars[6]=3;r=rt_actor_tick();check(!r&&!agi_vm_flag(&rt_vm,3),"leaving trigger clears flag 3");
    reset();seed(0,40,80,0,1);rt_actor_read(0,&a);a.width=3;rt_actor_write(0,&a);
    pixel(40,80,3);pixel(41,80,3);pixel(42,80,3);r=rt_actor_tick();
    check(!r&&agi_vm_flag(&rt_vm,0),"whole baseline in water sets flag 0");
    pixel(42,80,4);r=rt_actor_tick();check(!r&&!agi_vm_flag(&rt_vm,0),"mixed water and land is land");
    reset();seed(0,40,80,0,1);pixel(40,80,3);agi_pixel_write(80u*160+40,0xf0,0xf0);r=rt_actor_tick();
    check(!r&&agi_vm_flag(&rt_vm,0),"sprite priority cannot overwrite clean water detection");
    reset();seed(0,40,80,3,1);pixel(40,80,3);cmd(0x40,0,0,0);r=rt_actor_tick();
    check(!r&&at(0,40,80)&&agi_vm_flag(&rt_vm,0),"water-only actor cannot leave water");
    reset();seed(0,40,80,3,1);pixel(41,80,3);cmd(0x41,0,0,0);r=rt_actor_tick();
    check(!r&&at(0,40,80)&&!agi_vm_flag(&rt_vm,0),"land-only actor cannot enter water");
    cmd(0x42,0,0,0);r=rt_actor_tick();check(!r&&at(0,41,80),"object.on.anything releases terrain restriction");
    reset();seed(0,40,80,3,1);pixel(41,80,0);cmd(0x36,0,15,0);r=rt_actor_tick();
    check(!r&&at(0,41,80)&&!agi_vm_flag(&rt_vm,0)&&!agi_vm_flag(&rt_vm,3),"fixed priority 15 bypasses picture control pixels");
    reset();seed(0,30,51,0,1);rt_actor_read(0,&a);a.width=4;rt_actor_write(0,&a);
    pixel(30,51,3);pixel(31,51,3);pixel(32,51,3);pixel(33,51,3);r=rt_actor_tick();
    check(!r&&agi_vm_flag(&rt_vm,0),"baseline reads span an 8KB picture bank boundary");
}
static void objects(void) {
    uint8_t r;rt_actor a;
    reset();seed(0,40,80,3,1);seed(255,42,80,0,1);r=rt_actor_tick();
    check(!r&&at(0,40,80),"touching animated actor baseline blocks horizontal motion");
    cmd(0x43,255,0,0);r=rt_actor_tick();check(!r&&at(0,41,80),"obstacle ignore.objs exempts that actor");
    cmd(0x44,255,0,0);cmd(0x43,0,0,0);r=rt_actor_tick();check(!r&&at(0,42,80),"moving actor ignore.objs bypasses object collision");
    reset();seed(0,40,80,5,4);seed(1,40,83,0,1);cmd(0x3a,1,0,0);r=rt_actor_tick();
    check(!r&&at(0,40,80),"large vertical step cannot cross stopped actor baseline");
    reset();seed(0,40,80,5,4);seed(1,40,83,1,4);r=rt_actor_tick();
    check(!r&&at(0,40,80)&&at(1,40,83),"opposing actors cannot swap baselines in one tick");
    reset();seed(0,40,80,5,1);seed(1,40,83,0,1);r=rt_actor_tick();
    check(!r&&at(0,40,81),"overlapping sprite bodies may move at different baselines");
    cmd(0x24,1,0,0);rt_vm.vars[6]=5;rt_actor_read(0,&a);a.step=3;rt_actor_write(0,&a);r=rt_actor_tick();
    check(!r&&at(0,40,84),"erased actors do not block movement");
    seed(1,50,90,0,1);cmd(0x45,0,1,200);check(rt_vm.vars[200]==16,"distance uses baseline centres and Manhattan distance");
    cmd(0x24,1,0,0);cmd(0x45,0,1,200);check(rt_vm.vars[200]==255,"distance to undrawn actor is 255");
}
static void borders(void) {
    uint8_t r,i,saved3=ZXN_READ_REG(0x53),saved4=ZXN_READ_REG(0x54);rt_actor a;
    static const uint8_t coords[16]={50,37,1,1,159,80,3,2,50,167,5,3,0,80,7,4};
    for(i=0;i<4;++i) {
        reset();seed(0,coords[i*4],coords[i*4+1],coords[i*4+2],1);r=rt_actor_tick();
        check(!r&&rt_vm.vars[2]==coords[i*4+3],"ego edge event uses AGI top/right/bottom/left codes");
        rt_vm.vars[6]=0;r=rt_actor_tick();check(!r&&!rt_vm.vars[2],"edge event expires on following movement tick");
    }
    reset();seed(255,159,80,3,1);r=rt_actor_tick();
    check(!r&&!rt_vm.vars[2]&&rt_vm.vars[4]==255&&rt_vm.vars[5]==2,"non-ego edge records actor ID in v4 and edge in v5");
    reset();seed(0,1,80,7,2);pixel(0,80,0);r=rt_actor_tick();
    check(!r&&at(0,1,80)&&!rt_vm.vars[2],"blocked edge does not request room exit");
    reset();seed(0,50,2,1,1);rt_actor_read(0,&a);a.height=3;a.flags|=ACT_IGNORE_HORIZON;rt_actor_write(0,&a);r=rt_actor_tick();
    check(!r&&at(0,50,2)&&rt_vm.vars[2]==1,"top edge respects sprite height when horizon ignored");
    reset();seed(0,158,80,3,3);rt_actor_read(0,&a);a.motion=1;a.target_x=170;a.target_y=80;a.motion_flag=200;a.previous_step=1;rt_actor_write(0,&a);
    r=rt_actor_tick();rt_actor_read(0,&a);
    check(!r&&rt_vm.vars[2]==2&&agi_vm_flag(&rt_vm,200)&&!a.motion&&a.step==1,"move.obj edge completion restores original step size");
    reset();seed(0,40,80,3,1);rt_actor_read(0,&a);a.step_time=a.step_count=3;rt_actor_write(0,&a);
    rt_actor_tick();rt_actor_tick();check(at(0,40,80),"step.time delays movement for two ticks");r=rt_actor_tick();
    check(!r&&at(0,41,80),"step.time moves on third tick");
    check(ZXN_READ_REG(0x53)==saved3&&ZXN_READ_REG(0x54)==saved4,"collision and motion preserve data and code banks");
}
static void placement(void) {
    uint8_t r;rt_actor a;
    reset();seed(0,40,80,3,1);cmd(0x24,0,0,0);pixel(40,80,0);r=cmd(0x23,0,0,0);
    check(!r&&at(0,39,80),"draw adjusts blocked position using west-first spiral");
    r=cmd(0x93,0,40,80);if(!r)r=rt_actor_tick();
    check(!r&&at(0,39,80),"reposition.to adjusts and skips automatic movement once");
    rt_vm.vars[201]=50;rt_vm.vars[202]=90;r=cmd(0x94,0,201,202);if(!r)r=rt_actor_tick();
    check(!r&&at(0,50,90),"reposition.to.v uses variable coordinates");
    rt_vm.vars[201]=255;rt_vm.vars[202]=254;r=cmd(0x28,0,201,202);
    check(!r&&at(0,49,88),"reposition applies signed coordinate deltas");
    reset();seed(0,40,80,0,1);cmd(0x24,0,0,0);rt_actor_read(0,&a);a.width=161;rt_actor_write(0,&a);
    check(cmd(0x23,0,0,0)==VM_ARGUMENT,"invalid actor geometry fails bounded placement");
}
static void stamps(void) {
    uint8_t p[7]={2,0,0,50,80,4,4},r,i,saved=ZXN_READ_REG(0x53);
    reset();seed(0,100,100,0,1);rt.dirty=3;rt_actor_render();
    r=rt_command(&rt_vm,0x7a,p);
    check(!r&&(agi_pixel_get(80u*160+50)&15)==2,"add.to.pic places original cel in background");
    cmd(0x24,0,0,0);rt_actor_render();
    check((agi_pixel_get(100u*160+100)&15)==15,"permanent stamp does not bake existing actors into background");
    agi_pixel_write(80u*160+50,15,7);next_scene_restore(50,80,1,1);
    check((agi_pixel_get(80u*160+50)&15)==2,"scene restoration retains permanent stamp");
    reset();pixel(50,75,3);pixel(50,77,1);p[6]=0;r=rt_command(&rt_vm,0x7a,p);
    check(!r&&(agi_pixel_get(73u*160+50)>>4)==0&&(agi_pixel_get(80u*160+50)>>4)==0,"stamp control margin spans baseline priority band");
    check((agi_pixel_get(75u*160+50)>>4)==3&&(agi_pixel_get(77u*160+50)>>4)==1,"stamp margin preserves existing water and control lines");
    seed(0,49,76,3,1);r=rt_actor_tick();check(!r&&at(0,49,76),"stamped control margin participates in movement collision");
    p[3]=60;p[4]=90;p[5]=0;p[6]=4;
    for(i=0;i<7;++i){rt_vm.vars[210+i]=p[i];p[i]=210+i;}
    r=rt_command(&rt_vm,0x7b,p);
    check(!r&&agi_pixel_get(90u*160+60)==0x82,"add.to.pic.v resolves all operands and automatic priority");
    check(ZXN_READ_REG(0x53)==saved,"permanent stamps preserve data-window mapping");
}
static void geometry(void) {
    rt_actor a;uint8_t r;
    reset();seed(0,40,80,3,1);cmd(0x29,0,4,0);cmd(0x2b,0,1,0);cmd(0x2e,0,0,0);pixel(43,80,0);
    r=rt_actor_tick();rt_actor_read(0,&a);
    check(!r&&a.x==40&&a.width==3&&!a.loop,"new directional loop footprint is used for collision");
    reset();seed(0,48,80,7,2);seed(1,43,80,0,1);cmd(0x29,1,5,0);cmd(0x47,1,0,0);
    r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&at(0,48,80)&&a.width==3&&a.cel==1,"all actors select new cel geometry before any actor moves");
}
static void scripted_direction(void) {
    rt_actor a;uint8_t p[5]={0,43,84,2,201},r;
    reset();seed(0,40,80,0,1);
    r=rt_actor_command(0x51,p);rt_actor_read(0,&a);
    check(!r && a.direction==4 && rt_vm.vars[6]==4 && !rt.player_control && !(a.flags&ACT_CYCLING),"move.obj reports ego direction immediately without forcing cel cycling");
    r=rt_actor_tick();rt_actor_read(0,&a);
    check(!r && a.x==42 && a.y==82 && a.direction==4 && rt_vm.vars[6]==4,"scripted diagonal step keeps reported direction");
    r=rt_actor_tick();rt_actor_read(0,&a);
    check(!r && a.x==43 && a.y==84 && !a.direction && !rt_vm.vars[6] && rt.player_control && a.step==1 && agi_vm_flag(&rt_vm,201),"partial final step completes and clears direction with original step restored");
    seed(1,40,80,0,1);rt_vm.vars[6]=7;p[0]=1;p[1]=40;p[2]=90;
    r=rt_actor_command(0x51,p);rt_actor_read(1,&a);
    check(!r && a.direction==5 && rt_vm.vars[6]==7,"scripted non-ego direction does not overwrite ego direction");
}
uint8_t bank_motion_tests(void) {
    char path[40];uint8_t r;
    checks=failures=0;strcpy(path,"build/motion-tests.txt");file=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    strcpy(path,"build/runtime-fixtures");r=rt_init(path);check(!r,"collision fixtures initialize");
    if(!r){terrain();objects();borders();placement();stamps();geometry();scripted_direction();}
    sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",(unsigned)checks,(unsigned)failures);esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_close(file);
    return !failures;
}
