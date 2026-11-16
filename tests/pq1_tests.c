#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "next/sound.h"
#include "agi/picture.h"
#include "brush_cases.h"
static uint8_t file,checks,failures;
static void check(uint8_t ok,const char *name){++checks;if(!ok)++failures;sprintf(rt_buffer,"%s %s\n",ok?"PASS":"FAIL",name);esx_f_write(file,rt_buffer,strlen(rt_buffer));}
static uint8_t script(void){uint8_t r=agi_vm_start(&rt_vm,116);uint16_t n=0;while(r==VM_RUNNING && ++n<100)r=agi_vm_run(&rt_vm,200);return r;}
static uint8_t pixel(uint16_t x,uint8_t y){uint8_t m=ZXN_READ_REG(0x53),v;ZXN_WRITE_REG(0x53,40+(x>>5));v=*((uint8_t *)(0x6000u+((x&31)<<8)+y));ZXN_WRITE_REG(0x53,m);return v;}
static uint8_t rectangle_ok(void){uint16_t x;uint8_t y;for(x=79;x<=96;++x)for(y=71;y<=88;++y)if(pixel(x,y)!=(x>=80 && x<96 && y>=72 && y<88?15:3))return 0;return 1;}
static void rect(void){
 uint8_t p[5]={5,10,6,11,7},r;char path[16];
 strcpy(path,"games/pq1");check(!rt_init(path),"PQ1 initializes");
 next_screen_clear(3);rt.fg=14;rt.bg=2;strcpy(rt_buffer,"AB\nCD");rt_text_at(5,10,rt_buffer,1);
 r=rt_command(&rt_vm,0x9a,p);
 check(!r && rectangle_ok(),"clear.text.rect clears inclusive bounds to white without touching neighbours");
 check(rt.fg==14 && rt.bg==2,"rectangle clear preserves text attributes");
 next_screen_clear(3);rt_text_redraw();check(rectangle_ok(),"cleared rectangle survives text redraw without old glyphs");
 p[0]=24;p[1]=39;p[2]=p[3]=255;p[4]=0;r=rt_command(&rt_vm,0x9a,p);
 check(!r && pixel(312,224)==0 && pixel(319,231)==0 && pixel(311,224)==3 && pixel(319,232)==3,"oversized rectangle clips at text screen edge and supports black");
 p[0]=6;p[1]=10;p[2]=5;p[3]=11;p[4]=15;r=rt_command(&rt_vm,0x9a,p);check(!r && rectangle_ok(),"reversed rectangle is harmless");
}
static void shot(uint8_t page){char path[40];uint8_t f,m=ZXN_READ_REG(0x53),i;sprintf(path,"build/pq1-newspaper-%u.l2",page);f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);for(i=40;i<50;++i){ZXN_WRITE_REG(0x53,i);esx_f_write(f,(void *)0x6000,8192);}esx_f_close(f);ZXN_WRITE_REG(0x53,m);}
static void newspaper(void){
 char path[16];uint8_t r,i,p[2]={2,0};
 strcpy(path,"games/pq1");r=rt_init(path);rt_vm.vars[0]=116;
 r=script();check(!r && rt_vm.vars[33]==1,"original newspaper opens page one without STOP 10");shot(1);agi_vm_set_flag(&rt_vm,5,0);
 for(i=2;i<=4;++i){rt_vm.vars[6]=3;r=script();check(!r && rt_vm.vars[33]==i,"original newspaper turns to next page");shot(i);}
 rt_vm.vars[6]=3;r=script();check(!r && rt_vm.vars[33]==1,"newspaper wraps from last page to first");
 rt_vm.vars[6]=7;r=script();check(!r && rt_vm.vars[33]==4,"newspaper turns backward and wraps");
 rt_format(116,2,rt_buffer,sizeof(rt_buffer));check(strchr(rt_buffer,'|') && !strchr(rt_buffer,'\\'),"escaped newspaper column divider is one literal character");
 rt_vm.vars[6]=0;strcpy(rt.strings[2],"put down newspaper");r=rt_parser_command(0x75,p);if(!r)r=script();
 check(r==VM_ROOM && rt_vm.room==4,"original put-down command leaves newspaper for room four");
}
static void brushes(void){
 uint16_t i;uint8_t n,r,s,row,col,expected,ok=1,code[12];int x,y,left,top,xx,yy;
 for(i=0;i<sizeof(brush_cases)/sizeof(brush_cases[0]);++i){
  s=brush_cases[i].code&7;x=brush_cases[i].x;y=brush_cases[i].y;
  code[0]=0xf0;code[1]=2;code[2]=0xf2;code[3]=9;code[4]=0xf9;code[5]=brush_cases[i].code;code[6]=0xfa;n=7;
  if(code[5]&32)code[n++]=brush_cases[i].pattern*2;
  code[n++]=x;code[n++]=y;code[n++]=0xff;
  agi_picture_clear();r=agi_picture_decode(code,n);if(r){ok=0;sprintf(rt_buffer,"BRUSH %u decode=%u\n",i,r);esx_f_write(file,rt_buffer,strlen(rt_buffer));}
  if(x<s)x=s-1;if(y<s)y=s;left=x-(s+1)/2;top=y-s;
  for(row=0;row<2*s+3;++row)for(col=0;col<s+3;++col){
   xx=left+col-1;yy=top+row-1;if(xx<0 || xx>=160 || yy<0 || yy>=168)continue;
   expected=row && row<=2*s+1 && col && col<=s+1 && (brush_cases[i].rows[row-1]&(1u<<(col-1)))?0x92:0x4f;
   if(agi_pixel_get(yy*160+xx)!=expected){
    if(ok){sprintf(rt_buffer,"BRUSH %u code=%u at=%u,%u expected=%u actual=%u\n",i,brush_cases[i].code,(unsigned)xx,(unsigned)yy,expected,agi_pixel_get(yy*160+xx));esx_f_write(file,rt_buffer,strlen(rt_buffer));}
    ok=0;
   }
  }
 }
 check(ok,"128 brush footprints match row oracle: all sizes, shapes, textures and screen edges");
 code[0]=0xf9;code[1]=0x30;code[2]=0xfa;code[3]=0;code[4]=20;code[5]=0xff;
 check(agi_picture_decode(code,6)==PIC_TRUNCATED,"truncated textured brush does not read past its coordinate pair");
}
static void exterior(void){
 static const uint8_t cars[5][3]={{30,128,11},{32,150,13},{110,112,10},{112,128,11},{117,148,13}};
 char path[16];uint8_t r,p[3]={0,0,0},car,band;uint16_t n;rt_actor a;
 strcpy(path,"games/pq1");r=rt_init(path);rt_vm.vars[0]=7;rt_vm.vars[1]=1;
 if(!r)r=rt_command(&rt_vm,0x21,p);if(!r)r=rt_command(&rt_vm,0x29,p);
 if(!r){r=agi_vm_start(&rt_vm,7);for(n=0;r==VM_RUNNING && n<100;++n)r=agi_vm_run(&rt_vm,200);}
 check(!r && rt.picture_visible && rt.last_picture==7,"original LOGIC 7 completes station exterior initialization without STOP");shot(7);
 if(r)return;
 for(car=0;car<5;++car){
  band=(cars[car][1]/12)*12;
  rt_actor_read(0,&a);a.x=cars[car][0];a.y=band-3;a.direction=5;a.motion=0;
  a.step=a.step_time=a.step_count=1;a.flags&=~ACT_FIXED_PRIORITY;rt_actor_write(0,&a);rt_vm.vars[6]=5;
  for(n=0;n<5 && !r;++n){r=rt_actor_tick();if(!r)r=rt_actor_render();}
  rt_actor_read(0,&a);
  sprintf(rt_buffer,"CAR %u y=%u band=%u actor-priority=%u car-priority=%u\n",car,a.y,band,agi_view_priority(a.y),cars[car][2]);esx_f_write(file,rt_buffer,strlen(rt_buffer));
  check(!r && a.y==band-1 && agi_view_priority(a.y)<cars[car][2],"walking behind parked car stops before matching its priority");
  shot(10+car);
 }
}
static void pictures(void){
 resource_handle h;uint16_t i,total=0;uint8_t r;char path[16];
 strcpy(path,"games/pq1");r=rt_init(path);check(!r,"PQ1 picture audit initializes");
 for(i=0;i<256;++i){
  if(resource_acquire(RESOURCE_PICTURE,i,&h))continue;resource_release(h);++total;
  r=rt_picture(i,0);if(r){sprintf(rt_buffer,"FAIL PQ1 picture %u decode=%u\n",i,r);esx_f_write(file,rt_buffer,strlen(rt_buffer));++failures;}
 }
 check(total==71,"all 71 available PQ1 pictures decoded");
 sprintf(rt_buffer,"PICTURES %u\n",total);esx_f_write(file,rt_buffer,strlen(rt_buffer));
}
int main(void){next_init();next_sound_manual=1;next_clock_start();strcpy(rt_buffer,"build/pq1-tests.txt");file=esx_f_open(rt_buffer,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);rect();newspaper();brushes();if(!failures){exterior();pictures();}sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",checks,failures);esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_close(file);for(;;){}}
