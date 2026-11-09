#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "next/sound.h"
static uint8_t file,checks,failures;
static void logline(void){char path[40];uint8_t f;esx_f_write(file,rt_buffer,strlen(rt_buffer));strcpy(path,"build/bridge-progress.txt");f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);esx_f_write(f,rt_buffer,strlen(rt_buffer));esx_f_close(f);}
static void check(uint8_t ok,const char *name){++checks;if(!ok)++failures;sprintf(rt_buffer,"%s %s\n",ok?"PASS":"FAIL",name);logline();}
static uint8_t command(uint8_t op,uint8_t a,uint8_t b,uint8_t c){uint8_t p[7]={0};p[0]=a;p[1]=b;p[2]=c;return rt_command(&rt_vm,op,p);}
static uint8_t init(void){char path[20];strcpy(path,"games/sq1");return rt_init(path);}
static uint8_t logic(uint8_t number){uint8_t r;uint16_t n=0;r=agi_vm_start(&rt_vm,number);while((r==VM_RUNNING || r==VM_WAIT) && ++n<100){if(r==VM_WAIT){rt_key(13,0);agi_vm_resume(&rt_vm);}r=agi_vm_run(&rt_vm,1000);}return r;}
static void snapshot(const char *name){uint8_t f,p,m=ZXN_READ_REG(0x53);strcpy(rt_buffer,name);f=esx_f_open(rt_buffer,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);for(p=40;p<50;++p){ZXN_WRITE_REG(0x53,p);esx_f_write(f,(void *)0x6000,8192);}esx_f_close(f);ZXN_WRITE_REG(0x53,m);}
static uint8_t text(uint8_t row,uint8_t col,uint8_t ch){uint8_t m=ZXN_READ_REG(0x53),ok;ZXN_WRITE_REG(0x53,61);ok=*((uint8_t *)(0x6000u+row*40u+col))==ch;ZXN_WRITE_REG(0x53,m);return ok;}
static void ego(void){command(0x21,0,0,0);command(0x29,0,0,0);command(0x25,0,120,150);}
static void autonav(void){uint16_t n;uint8_t r=0;rt_actor a;
    check(!init(),"SQ1 initializes for Autonav");rt_picture(104,0);rt.picture_visible=1;rt_redraw();
    agi_vm_set_flag(&rt_vm,5,1);check(!logic(111),"original planet profile initializes");agi_vm_set_flag(&rt_vm,5,0);
    for(n=0;n<160 && !r;++n){r=logic(111);if(!r)r=rt_actor_tick();if(!r)r=rt_actor_render();}
    check(!r && text(3,19,'P') && text(4,21,'K') && text(19,6,'N'),"original Autonav displays planet details and navigation status");
    rt_actor_read(11,&a);check(!a.cycle && (a.flags&ACT_CYCLING),"planet continues normal cycling after reveal completes");snapshot("build/kerona-autonav.l2");
}
static void cave(void){uint8_t r,n;rt_actor a;
    init();ego();rt_vm.vars[0]=26;rt_vm.vars[1]=25;agi_vm_set_flag(&rt_vm,5,1);
    r=logic(26);agi_vm_set_flag(&rt_vm,5,0);check(!r,"original geyser room initializes");
    rt_actor_read(4,&a);check(!(a.flags&ACT_UPDATE) && !a.cel,"closed hidden door starts stopped");
    /* Execute the original successful put-rock branch, after its parser tests. */
    rt.locations[2]=255;rt_vm.scan[26]=819;r=logic(26);rt_vm.scan[26]=0;
    check(!r && agi_vm_flag(&rt_vm,84),"original put-rock branch unlocks door");
    rt_actor_read(4,&a);check((a.flags&(ACT_UPDATE|ACT_CYCLING))==(ACT_UPDATE|ACT_CYCLING),"end.of.loop restarts stopped door updates");
    for(n=0;n<12 && !r;++n){r=rt_actor_tick();if(!r)r=rt_actor_render();if(!r)r=logic(26);}
    rt_actor_read(4,&a);check(!r && a.cel==5 && !(a.flags&ACT_UPDATE),"door reaches open cel and original script stops updates");snapshot("build/kerona-door.l2");
    init();ego();rt_vm.vars[0]=27;rt_vm.vars[1]=26;agi_vm_set_flag(&rt_vm,5,1);r=logic(27);agi_vm_set_flag(&rt_vm,5,0);
    sprintf(rt_buffer,"CAVE result=%u logic=%u pc=%u op=%02X\n",r,rt_vm.logic,rt_vm.pc,rt_vm.opcode);logline();
    check(!r,"original room 27 accepts offscreen waterfall setup");
    for(n=0;n<80 && !r;++n){r=rt_actor_tick();if(!r)r=logic(27);}
    check(!r,"room 27 waterfall runs for 80 cycles without STOP");if(!r){rt_actor_render();snapshot("build/kerona-cave.l2");}
}
static void spider(uint8_t wander){uint16_t n,moved=0,directions=0;uint8_t r,oldx,oldy;rt_actor a;
    init();ego();rt_vm.vars[0]=16;rt_vm.vars[1]=15;rt_picture(16,0);agi_vm_set_flag(&rt_vm,5,1);r=logic(110);agi_vm_set_flag(&rt_vm,5,0);agi_vm_set_flag(&rt_vm,101,1);agi_vm_set_flag(&rt_vm,92,wander);
    for(n=0;n<100 && !r && !agi_vm_flag(&rt_vm,97);++n){r=logic(110);if(!r)r=rt_actor_tick();}
    check(!r && agi_vm_flag(&rt_vm,97),wander?"original droid landing reaches wander":"original droid landing reaches pursuit");
    rt_actor_read(16,&a);check(a.motion==(wander?RT_MOTION_WANDER:RT_MOTION_FOLLOW) && !a.cycle,"droid leaves landing cycle for requested motion");
    for(n=0;n<120 && !r;++n){oldx=a.x;oldy=a.y;r=rt_actor_tick();rt_actor_read(16,&a);if(a.x!=oldx || a.y!=oldy)++moved;directions|=1u<<a.direction;}
    sprintf(rt_buffer,"DROID wander=%u moved=%u directions=%03X x=%u y=%u\n",wander,moved,directions,a.x,a.y);logline();
    check(!r && moved>15,wander?"droid wanders across the terrain":"droid actively pursues Roger");
}
static void bridge(void){
    rt_actor a;uint16_t n,horizontal=0,longest=0,diagonal=0,first=65535,visits=0,gap=0,maxgap=0,started,frames=0;uint8_t r,oldx,oldy,olddir,miny=167,maxy=0,inzone=0;
    init();ego();rt_vm.vars[0]=19;rt_vm.vars[1]=18;
    agi_vm_set_flag(&rt_vm,92,1);agi_vm_set_flag(&rt_vm,97,1);agi_vm_set_flag(&rt_vm,191,1);agi_vm_set_flag(&rt_vm,5,1);
    r=logic(19);check(!r,"rock bridge initializes");agi_vm_set_flag(&rt_vm,5,0);agi_vm_set_flag(&rt_vm,102,1);
    command(0x25,0,47,62);command(0x29,16,46,0);command(0x25,16,32,144);command(0x23,16,0,0);command(0x54,16,0,0);
    rt_actor_read(16,&a);a.direction=8;a.target_x=231;a.ignore_blocks|=ACT_ON_LAND;rt_actor_write(16,&a);
    for(n=0;n<1200 && !r;++n){
        oldx=a.x;oldy=a.y;olddir=a.direction;started=next_clock();r=logic(19);if(!r)r=rt_actor_tick();
        if(n<60){if(!r)r=rt_actor_render();frames+=next_clock()-started;}
        rt_actor_read(16,&a);
        if(a.x!=oldx && a.y!=oldy)++diagonal;
        if(a.y==oldy)++horizontal;else horizontal=0;if(horizontal>longest)longest=horizontal;
        if(a.y<miny)miny=a.y;if(a.y>maxy)maxy=a.y;
        if(a.x>=36 && a.x<=58 && a.y>=137 && a.y<=151){if(first==65535)first=n;if(!inzone)++visits;inzone=1;gap=0;}
        else {inzone=0;if(++gap>maxgap)maxgap=gap;}
        if(olddir!=a.direction){sprintf(rt_buffer,"BRIDGE tick=%u x=%u y=%u dir=%u count=%u rng=%u\n",n,a.x,a.y,a.direction,a.target_x,rt.random);logline();}
    }
    sprintf(rt_buffer,"BRIDGE result=%u miny=%u maxy=%u longest-flat=%u diagonal=%u\n",r,miny,maxy,longest,diagonal);logline();
    check(!r,"original rock-bridge droid runs 1200 cycles");
    check(diagonal>100 && maxy-miny>40,"rock-bridge droid makes diagonal progress across the lower area");
    sprintf(rt_buffer,"CRUSH WINDOW first=%u visits=%u longest-absence=%u ticks\n",first,visits,maxgap);logline();
    sprintf(rt_buffer,"BRIDGE CPU first 60 room-script/actor/render cycles=%u emulated 50Hz frames; excludes main LOGIC 0, parser, pacing and trace IO\n",frames);logline();
}
/* Execute only the original main LOGIC's droid-arrival timer, stopping before
   its unrelated thirst/parser/UI sections. Offsets include the two-byte header. */
static uint8_t arrival_timer(void){uint16_t n=0;uint8_t r;rt_vm.scan[0]=1331;r=agi_vm_start(&rt_vm,0);
    while(r==VM_RUNNING && rt_vm.frames[0].pc<1379 && n++<100)r=agi_vm_run(&rt_vm,1);
    agi_vm_stop(&rt_vm);rt_vm.scan[0]=0;return r==VM_RUNNING?VM_DONE:r;
}
static void crush(uint8_t pending){
    uint8_t r,n,landed=0;rt_actor a;uint16_t at;
    init();ego();rt_vm.vars[0]=19;rt_vm.vars[1]=18;
    agi_vm_set_flag(&rt_vm,92,1);agi_vm_set_flag(&rt_vm,97,1);agi_vm_set_flag(&rt_vm,191,1);
    r=logic(19);agi_vm_set_flag(&rt_vm,5,0);agi_vm_set_flag(&rt_vm,102,1);
    command(0x25,0,47,62);command(0x29,16,46,0);command(0x25,16,41,143);command(0x23,16,0,0);
    agi_vm_set_flag(&rt_vm,96,pending);rt_vm.vars[94]=20;rt_vm.vars[95]=2;
    /* Successful push-rock action, after the original parser/position tests. */
    rt_vm.scan[19]=956;r=logic(19);rt_vm.scan[19]=0;
    for(n=0;n<30 && !r && !agi_vm_flag(&rt_vm,165);++n){r=rt_actor_tick();if(!r)r=logic(19);}
    check(!r && agi_vm_flag(&rt_vm,165) && agi_vm_flag(&rt_vm,161),"original crush completes and awards destruction flags");
    check(!agi_vm_flag(&rt_vm,97) && !agi_vm_flag(&rt_vm,101) && agi_vm_flag(&rt_vm,96)==pending,"crush clears active droid but leaves any queued arrival timer");
    for(at=0;at<300 && !r;++at){r=arrival_timer();if(!r)r=logic(19);if(!r)r=rt_actor_tick();rt_actor_read(16,&a);if(a.view==48 && (a.flags&ACT_DRAWN))landed=1;}
    check(!r && landed==pending,pending?"original queued timer spawns a replacement after crushing":"no replacement appears without a queued arrival timer");
}
int main(void){next_init();next_sound_manual=1;next_clock_start();strcpy(rt_buffer,"build/kerona-tests.txt");file=esx_f_open(rt_buffer,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);crush(0);crush(1);bridge();autonav();cave();spider(0);spider(1);sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",checks,failures);logline();esx_f_close(file);for(;;){} }
