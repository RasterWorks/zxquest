#include <arch/zxn/esxdos.h>
#include <arch/zxn.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "next/sound.h"
static uint8_t file,checks,failures;
static void logline(void){esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_sync(file);}
static void check(uint8_t ok,const char *name){++checks;if(!ok)++failures;sprintf(rt_buffer,"%s %s\n",ok?"PASS":"FAIL",name);logline();}
static uint8_t cmd(uint8_t op,uint8_t a,uint8_t b,uint8_t c){uint8_t p[7]={0};p[0]=a;p[1]=b;p[2]=c;return rt_command(&rt_vm,op,p);}
static uint8_t scan(void){uint8_t r;uint16_t n=0;do{r=rt_step(n?0:5);++n;}while(r==VM_RUNNING && n<500);return r;}
static void dump(const char *name,uint8_t page,uint16_t bytes){
 char path[64];uint8_t f,m=ZXN_READ_REG(0x53);uint16_t n;
 strcpy(path,name);f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
 while(bytes){n=bytes>8192?8192:bytes;ZXN_WRITE_REG(0x53,page++);esx_f_write(f,(void *)0x6000,n);bytes-=n;}
 esx_f_close(f);ZXN_WRITE_REG(0x53,m);
}
static uint8_t script(uint8_t logic){uint8_t r=agi_vm_start(&rt_vm,logic);uint16_t n=0;while(r==VM_RUNNING && ++n<500)r=agi_vm_run(&rt_vm,200);return r;}
static uint16_t picture_hash(uint8_t page){uint8_t m=ZXN_READ_REG(0x53);uint16_t at,n,left=26880,h=0;while(left){n=left>8192?8192:left;ZXN_WRITE_REG(0x53,page++);for(at=0;at<n;++at)h=(h<<1)|(h>>15),h+=*((uint8_t *)(0x6000u+at));left-=n;}ZXN_WRITE_REG(0x53,m);return h;}
static void layouts(void){
 char path[48];uint8_t r,i,j,p[3];uint16_t expected,cycle;rt_actor a;
 const char *games[3]={"kq1","sq1","pq1"};
 for(j=0;j<3;++j){
  sprintf(path,"games/%s",games[j]);r=rt_init(path);
  for(cycle=0;cycle<3000 && !r && !rt.menus_ready;++cycle){r=scan();if(r==VM_WAIT){rt_key(13,0);r=0;}}
  check(!r && rt.menus_ready,"original game defines menus");
  if(rt.ui)rt_key(27,0);
  p[0]=0;r=rt_text_command(0xa1,p);
  for(i=0;i<rt.menu_count && r==VM_WAIT;++i){sprintf(path,"build/%s-menu-%u.l2",games[j],i);/* Full Layer 2 is 80KB. */
   {uint8_t f,m=ZXN_READ_REG(0x53),pg;f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);for(pg=40;pg<50;++pg){ZXN_WRITE_REG(0x53,pg);esx_f_write(f,(void *)0x6000,8192);}esx_f_close(f);ZXN_WRITE_REG(0x53,m);}
   rt_ui_key(0,0x4d);
  }
  check(r==VM_WAIT && rt.menu_selected==0,"all menu headings and dropdowns remain selectable");rt_ui_key(27,0);
 }
 strcpy(path,"games/kq1");rt_init(path);strcpy(rt.game_id,"KQ1");
 p[1]=0;p[0]='=';p[2]=5;rt_command(&rt_vm,0x79,p);p[0]='-';p[2]=8;rt_command(&rt_vm,0x79,p);p[0]='0';p[2]=7;rt_command(&rt_vm,0x79,p);
 rt_key(';',0);rt_key(',',0);rt_key('.',0);
 check((rt.pending[0]&160)==160 && (rt.pending[1]&1),"Next punctuation dispatches original swim duck jump controllers");
 memset(rt.pending,0,32);rt_key('0',0);rt_key('-',0);rt_key('=',0);
 check(!rt.pending[0] && !rt.pending[1],"old punctuation no longer triggers KQ1 actions");
 r=rt_format(0,159,rt_buffer,sizeof(rt_buffer));check(!r && strstr(rt_buffer,"<E-Q>"),"KQ1 Quit label uses Edit Q");
 r=rt_format(0,158,rt_buffer,sizeof(rt_buffer));check(!r && strstr(rt_buffer,"<BREAK>"),"KQ1 Pause label uses Break");
 rt_picture(53,0);rt.picture_visible=1;next_show_picture(0);
 rt_vm.logic=53;p[0]=57;r=rt_text_command(0x65,p);check(r==VM_WAIT,"original ending credits display");
 r=rt_format(53,57,rt_buffer,sizeof(rt_buffer));check(!r && strstr(rt_buffer,"Thank you for playing King's Quest\n") && strstr(rt_buffer,"from\n"),"ending heading fits one line with from beneath");
 {uint8_t f,m=ZXN_READ_REG(0x53),pg;strcpy(path,"build/kq1-credits.l2");f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);for(pg=40;pg<50;++pg){ZXN_WRITE_REG(0x53,pg);esx_f_write(f,(void *)0x6000,8192);}esx_f_close(f);ZXN_WRITE_REG(0x53,m);}
 rt_ui_key(27,0);
 /* Overlay a beanstalk over the live actor, then compare the clean picture
    against an independent overlay with no actor ever drawn. */
 rt_picture(6,0);cmd(0x21,0,0,0);cmd(0x29,0,0,0);cmd(0x25,0,70,140);cmd(0x23,0,0,0);rt_actor_render();
 r=rt_picture(84,1);expected=picture_hash(36);cmd(0x24,0,0,0);rt_actor_render();
 check(!r && picture_hash(32)==expected,"erasing Graham after beanstalk overlay restores clean pixels");
 rt_picture(6,0);rt_picture(84,1);check(picture_hash(36)==expected,"beanstalk background never captures Graham's sprite");
 rt_actor_reset(1);cmd(0x21,0,0,0);cmd(0x29,0,0,0);rt_vm.vars[0]=14;rt_vm.vars[1]=63;agi_vm_set_flag(&rt_vm,5,1);agi_vm_set_flag(&rt_vm,131,0);r=script(14);rt_actor_read(0,&a);
 check(!r && a.x==84 && a.y==132,"original DOS tree descent places Graham at 84,132");
 for(i=70;i<=72;++i){rt_picture(i,0);sprintf(path,"build/kq1-picture-%u.raw",i);dump(path,32,26880);}
 rt_picture(70,0);rt_actor_reset(1);cmd(0x21,0,0,0);cmd(0x29,0,74,0);cmd(0x25,0,69,160);cmd(0x23,0,0,0);rt_vm.vars[6]=0;rt_actor_tick();rt_actor_read(0,&a);
 check(a.width==8 && a.x==69 && a.y==160 && !agi_vm_flag(&rt_vm,3),"authored beanstalk entry fits its eight-pixel safe corridor");
 cmd(0x25,0,76,160);rt_actor_tick();
 check(agi_vm_flag(&rt_vm,3),"right edge of Graham's baseline touching P2 triggers original fall flag");
}
static void cycles(void){
 uint8_t r=0,n,p[5]={0,120,110,1,201};rt_actor a;char path[16];
 strcpy(path,"games/kq1");r=rt_init(path);rt.horizon=36;
 r|=cmd(0x21,0,0,0);r|=cmd(0x29,0,75,0);r|=cmd(0x25,0,60,110);r|=cmd(0x23,0,0,0);
 r|=cmd(0x49,0,144,0);r|=rt_actor_command(0x51,p);
 for(n=0;n<30 && !r;++n)r=rt_actor_tick();
 rt_actor_read(0,&a);
 check(!r && !agi_vm_flag(&rt_vm,144) && !a.cycle,"move.obj preserves animation but suppresses obsolete jump event");
 cmd(0x4d,0,0,0);cmd(0x2f,0,0,0);cmd(0x49,0,144,0);
 for(n=0;n<30 && !r && !agi_vm_flag(&rt_vm,144);++n)r=rt_actor_tick();
 check(!r && agi_vm_flag(&rt_vm,144),"a new one-shot animation still signals its own completion");
 cmd(0x4b,0,145,0);
 for(n=0;n<30 && !r && !agi_vm_flag(&rt_vm,145);++n)r=rt_actor_tick();
 check(!r && agi_vm_flag(&rt_vm,145),"ordinary reverse.loop still completes");
}
static void condor(void){
 uint8_t r,flight=0,old=255,stale=0;uint16_t n;rt_actor a;char path[16];
 strcpy(path,"games/kq1");r=rt_init(path);check(!r,"KQ1 initializes");
 rt_vm.vars[0]=22;rt_vm.vars[31]=4;rt_vm.vars[88]=2;rt.horizon=41;
 agi_vm_set_flag(&rt_vm,5,0);agi_vm_set_flag(&rt_vm,11,0);
 r=rt_picture(22,0);rt.picture_visible=1;
 r|=cmd(0x21,0,0,0);r|=cmd(0x29,0,75,0);r|=cmd(0x25,0,60,110);r|=cmd(0x23,0,0,0);
 rt_vm.vars[102]=2;r|=cmd(0x4c,0,102,0);r|=cmd(0x49,0,144,0);
 agi_vm_set_flag(&rt_vm,143,1);agi_vm_set_flag(&rt_vm,21,1);
 check(!r,"jump animation staged before condor catches Graham");
 /* Enter the original successful catch branch; do not alter game resources. */
 rt_vm.scan[22]=516;r=agi_vm_start(&rt_vm,22);
 while(r==VM_RUNNING)r=agi_vm_run(&rt_vm,1000);rt_vm.scan[22]=0;
 rt_actor_read(0,&a);check(!r && a.view==89 && a.motion==RT_MOTION_MOVE && !rt.player_control,"original catch starts condor flight");
 for(n=0;n<350 && (r==VM_DONE || r==VM_WAIT);++n){
  if(r==VM_WAIT)rt_key(13,0);
  r=scan();rt_actor_read(0,&a);
  if(agi_vm_flag(&rt_vm,144))stale=1;
  if(rt_vm.vars[0]!=old){sprintf(rt_buffer,"room=%u view=%u x=%u y=%u step=%u control=%u result=%u\n",rt_vm.vars[0],a.view,a.x,a.y,a.step,rt.player_control,r);logline();old=rt_vm.vars[0];}
  if(rt_vm.vars[0]==80)flight=1;
  if(rt_vm.vars[0]==48 && rt.player_control && a.view==0)break;
 }
 check(!stale,"catch does not fire the superseded jump completion flag");
 check(flight,"original script reaches flying cutscene in room 80");
 check(rt_vm.vars[0]==48 && rt.player_control && a.view==0,"condor lands Graham in destination room with player control");
 check(a.step==1,"Graham returns to normal walking step after flight");
}
int main(void){next_init();next_sound_manual=1;next_clock_start();strcpy(rt_buffer,"build/kq1-tests.txt");file=esx_f_open(rt_buffer,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);cycles();condor();layouts();sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",checks,failures);logline();esx_f_close(file);for(;;){}}
