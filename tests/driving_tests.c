#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "next/sound.h"
#include "next/save.h"
#include "agi/picture.h"
extern uint8_t rt_profile_on;
extern uint16_t rt_render_profile[5];
static uint8_t file,checks,failures;
static void logline(void){esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_sync(file);}
static void check(uint8_t ok,const char *name){++checks;if(!ok)++failures;sprintf(rt_buffer,"%s %s\n",ok?"PASS":"FAIL",name);logline();}
static uint8_t script(uint8_t logic){uint8_t r=agi_vm_start(&rt_vm,logic);uint16_t n=0;while(r==VM_RUNNING && ++n<300)r=agi_vm_run(&rt_vm,200);return r;}
static uint8_t command(uint8_t op,uint8_t id,uint8_t b,uint8_t c){uint8_t p[7]={0};p[0]=id;p[1]=b;p[2]=c;return rt_command(&rt_vm,op,p);}
static uint8_t scan(void){uint8_t r;uint16_t n=0;do{r=rt_step(n?0:10);++n;}while(r==VM_RUNNING && n<1000);return r;}
static uint8_t sentence(const char *s){while(*s)rt_key(*s++,0);rt_key(13,0);return scan();}
static uint8_t map_label(void){uint8_t saved=ZXN_READ_REG(0x53),yes;ZXN_WRITE_REG(0x53,61);yes=!memcmp((void *)0x6014,"Map A3",6);ZXN_WRITE_REG(0x53,saved);return yes;}
static void restored(void){
 char path[16];uint8_t r,i,dir;uint16_t at,elapsed;
 strcpy(path,"games/pq1");r=rt_init(path);if(!r)r=rt_save_slot(5,1);
 check(!r && rt_vm.vars[0]==12 && rt_vm.vars[10]==0,"Ccarrols restores in driving room at Fastest");if(r)return;
 r=scan();check(!r && map_label(),"restored driving status shows the correct map sector");
 for(dir=3;dir<=7 && !r;dir+=4){
  rt_vm.vars[6]=dir;at=next_clock();
  for(i=0;i<16 && !r;++i)r=scan();
  elapsed=next_clock()-at;sprintf(rt_buffer,"SAVED DRIVE direction=%u cycles=%u frames=%u result=%u room=%u\n",dir,i,elapsed,r,rt_vm.vars[0]);logline();
  check(!r,"saved game drives through complete interpreter cycles");
 }
 r=rt_save_slot(2,1);check(!r && rt_vm.vars[0]==7 && !map_label(),"walking restore clears the previous driving map label");
 if(!r)r=scan();check(!r && !map_label(),"walking status remains free of the driving label after a scan");
 r=rt_save_slot(8,1);if(!r)r=scan();if(!r)r=sentence("park");
 check(!r && rt_vm.vars[0]==35 && !map_label(),"parking at cafe clears driving sector from the walking screen");
}
static uint8_t street(uint8_t room){
 char path[16];uint8_t r;uint16_t start;
 strcpy(path,"games/pq1");r=rt_init(path);if(r)return r;
 rt_vm.vars[0]=room;rt_vm.vars[1]=7;rt_vm.vars[44]=99;rt_vm.vars[45]=100;rt_vm.vars[55]=1;
 rt_vm.vars[67]=255;rt_vm.vars[75]=30;rt_vm.vars[76]=36;rt_vm.vars[77]=0;
 start=next_clock();r=script(100);if(!r)r=script(117);if(!r)r=script(room);
 sprintf(rt_buffer,"LOAD room=%u frames=%u result=%u\n",room,next_clock()-start,r);logline();
 agi_vm_set_flag(&rt_vm,5,0);return r;
}
static void profile(uint8_t separated){
 uint8_t r=0,i,id;uint16_t start,tick=0,draw=0,at;rt_actor a;
 rt_actor_reset(1);rt.dirty=3;
 for(id=0;id<3;++id){command(0x21,id,0,0);command(0x29,id,103,0);command(0x25,id,30+id*40,separated?36+id*45:36);command(0x36,id,5,0);command(0x43,id,0,0);command(0x58,id,0,0);command(0x46,id,0,0);command(0x23,id,0,0);}
 r=rt_actor_render();memset(rt_render_profile,0,10);rt_profile_on=1;start=next_clock();
 for(i=0;i<16 && !r;++i){
  /* Equal actor count/displacement; only their vertical distribution differs. */
  at=next_clock();for(id=0;id<3;++id){rt_actor_read(id,&a);a.x+=(i&1)?-1:1;rt_actor_write(id,&a);}tick+=next_clock()-at;
  at=next_clock();r=rt_actor_render();draw+=next_clock()-at;
 }
 rt_profile_on=0;
 sprintf(rt_buffer,"PROFILE separated=%u frames=%u actors=%u render=%u restore=%u blit=%u upload=%u width=%u height=%u result=%u\n",separated,next_clock()-start,tick,draw,rt_render_profile[0],rt_render_profile[1],rt_render_profile[2],rt_render_profile[3],rt_render_profile[4],r);logline();
 check(!r,"traffic composition completes");
}
static void edge(void){
 uint8_t r,p[5]={1,120,80,1,201};rt_actor a;
 rt_actor_reset(1);rt.picture_valid=0;
 command(0x21,1,0,0);command(0x29,1,103,0);command(0x2b,1,2,0);
 command(0x25,1,159,80);command(0x46,1,0,0);command(0x23,1,0,0);
 rt_actor_read(1,&a);a.ignore_blocks&=~ACT_REPOSITIONED;rt_actor_write(1,&a);
 r=rt_actor_command(0x51,p);if(!r)r=rt_actor_tick();rt_actor_read(1,&a);
 check(!r&&a.x==157&&a.width==3&&a.motion==RT_MOTION_MOVE&&!rt_vm.vars[5]&&!agi_vm_flag(&rt_vm,201),"turning a narrow car at screen edge does not complete its route");
 r=rt_actor_tick();rt_actor_read(1,&a);
 check(!r&&a.x==156&&a.motion==RT_MOTION_MOVE,"car continues towards target after edge adjustment");
}
static void traffic(uint8_t room){
 uint8_t r,i,previous,changes=0;uint16_t n,start,vm=0,tick=0,draw=0,at,v,t,d,total,peak=0;rt_actor a;
 r=street(room);check(!r,"original driving street initializes");if(r)return;
 /* Park ego safely outside the authored traffic routes. */
 command(0x25,0,45,115);rt_vm.vars[6]=0;rt_vm.vars[222]=rt_vm.vars[223]=0;
 previous=rt_vm.vars[234];start=next_clock();
 for(n=0;n<280 && !r;++n){
  at=next_clock();r=script(117);if(!r)r=script(room);v=next_clock()-at;vm+=v;t=d=0;
  if(!r){at=next_clock();r=rt_actor_tick();t=next_clock()-at;tick+=t;}
  if(!r){memset(rt_render_profile,0,10);rt_profile_on=1;at=next_clock();r=rt_actor_render();d=next_clock()-at;draw+=d;rt_profile_on=0;}
  total=v+t+d;
  if(total>peak || rt_vm.vars[234]!=previous){
   if(total>peak)peak=total;
   sprintf(rt_buffer,"PEAK room=%u cycle=%u frames=%u vm=%u actors=%u render=%u restore=%u blit=%u upload=%u width=%u height=%u light=%u\n",room,n,total,v,t,d,rt_render_profile[0],rt_render_profile[1],rt_render_profile[2],rt_render_profile[3],rt_render_profile[4],rt_vm.vars[234]);logline();
  }
  if(rt_vm.vars[234]!=previous){previous=rt_vm.vars[234];++changes;sprintf(rt_buffer,"LIGHT cycle=%u phase=%u frames=%u\n",n,previous,next_clock()-start);logline();}
  if(n%40==0){for(i=1;i<=3;++i){rt_actor_read(i,&a);sprintf(rt_buffer,"CAR cycle=%u id=%u pos=%u,%u target=%u,%u dir=%u motion=%u state=%u flags=%u\n",n,i,a.x,a.y,a.target_x,a.target_y,a.direction,a.motion,rt_vm.vars[220+i],a.flags);logline();}}
 }
 sprintf(rt_buffer,"TRAFFIC room=%u cycles=%u frames=%u vm=%u actors=%u render=%u peak=%u result=%u\n",room,n,next_clock()-start,vm,tick,draw,peak,r);logline();
 check(!r && changes>=3,"authored traffic routes and light phases run without stopping");
}
static void dispatch(void){
 char path[16];uint8_t r,p[2]={8,0},state,found,wrong,i,saved;
 for(state=0;state<3;++state){
  strcpy(path,"games/pq1");r=rt_init(path);rt_vm.vars[0]=15;rt_vm.vars[67]=0;
  rt_vm.vars[92]=state?198+state:0;agi_vm_set_flag(&rt_vm,5,0);
  strcpy(rt.strings[2],"83-32,");strcpy(rt.strings[8],"call");
  r=rt_parser_command(0x75,p);if(!r)r=script(96);found=wrong=0;
  for(i=0;r==VM_WAIT && i<8;++i){
   saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,61);memcpy(rt_buffer,(void *)0x7000,768);ZXN_WRITE_REG(0x53,saved);rt_buffer[767]=0;
   if(strstr(rt_buffer,state==1?"en route to traffic":"10-98"))found=1;
   if(strstr(rt_buffer,state==1?"10-98":"en route to traffic"))wrong=1;
   rt_key(13,0);agi_vm_resume(&rt_vm);r=VM_RUNNING;
   while(r==VM_RUNNING)r=agi_vm_run(&rt_vm,200);
  }
  check(!r && found && !wrong,state==1?"original dispatch selects en-route text at accident state 199":"original dispatch selects 10-98 outside accident state 199");
 }
}
static void routes(void){
 uint8_t room,id,r,ok=1,stall[7],oldx[7],oldy[7];uint16_t cycle;rt_actor a;
 for(room=10;room<=25;++room){
  r=street(room);memset(stall,0,7);memset(oldx,0,7);memset(oldy,0,7);
  command(0x24,0,0,0);rt_vm.vars[6]=0;
  for(cycle=0;cycle<420 && !r;++cycle){
   r=script(117);if(!r)r=script(room);if(!r)r=rt_actor_tick();
   for(id=1;id<=6;++id){
    rt_actor_read(id,&a);
    /* Authored red-light waits deliberately observe the blue control line
       for up to 255 updates. Check free-moving cars, not those waits. */
    if((a.flags&(ACT_ANIMATED|ACT_DRAWN|ACT_UPDATE))==7 && (a.ignore_blocks&ACT_IGNORE_BLOCKS) && a.motion==RT_MOTION_MOVE && a.x==oldx[id] && a.y==oldy[id]){
     if(++stall[id]==64){sprintf(rt_buffer,"STUCK room=%u actor=%u pos=%u,%u target=%u,%u view=%u loop=%u dir=%u width=%u height=%u\n",room,id,a.x,a.y,a.target_x,a.target_y,a.view,a.loop,a.direction,a.width,a.height);logline();ok=0;}
    }else stall[id]=0;
    oldx[id]=a.x;oldy[id]=a.y;
   }
  }
  if(r){sprintf(rt_buffer,"ROUTE room=%u result=%u logic=%u pc=%u\n",room,r,rt_vm.logic,rt_vm.pc);logline();ok=0;}
 }
 check(ok,"traffic paths on all sixteen driving maps avoid sustained movement stalls");
}
int main(void){next_init();next_sound_manual=1;next_clock_start();strcpy(rt_buffer,"build/driving-tests.txt");file=esx_f_open(rt_buffer,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);restored();check(!street(12),"driving initializes");profile(0);profile(1);edge();traffic(12);traffic(16);
#ifndef DRIVING_PROFILE
dispatch();routes();
#endif
sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",checks,failures);logline();esx_f_close(file);for(;;){}}
