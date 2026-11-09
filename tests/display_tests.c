#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "agi/picture.h"
static uint8_t file,checks,failures,p[7];
static char path[40],report[160];
extern uint8_t rt_profile_on;
extern uint16_t rt_render_profile[5];
static void log(void){esx_f_write(file,report,strlen(report));}
static void check(uint8_t ok,const char *s){++checks;if(!ok)++failures;sprintf(report,"%s %s\n",ok?"PASS":"FAIL",s);log();}
static uint8_t cmd(uint8_t op,uint8_t a,uint8_t b,uint8_t c){memset(p,0,7);p[0]=a;p[1]=b;p[2]=c;return rt_command(&rt_vm,op,p);}
static uint8_t script(uint8_t n){uint8_t r=agi_vm_start(&rt_vm,n);uint16_t i=0;while(r==VM_RUNNING && ++i<100)r=agi_vm_run(&rt_vm,200);return r;}
static void shot(const char *s){uint8_t f,i,m=ZXN_READ_REG(0x53);strcpy(path,s);f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);for(i=40;i<50;++i){ZXN_WRITE_REG(0x53,i);esx_f_write(f,(void *)0x6000,8192);}esx_f_close(f);ZXN_WRITE_REG(0x53,m);}
static uint8_t init(uint8_t room){strcpy(path,"games/sq1");if(rt_init(path))return 1;rt_vm.vars[0]=room;cmd(0x21,0,0,0);return script(room);}
static uint16_t region(uint16_t left,uint16_t right,uint8_t top,uint8_t bottom){uint16_t x,h=0;uint8_t y,m=ZXN_READ_REG(0x53);for(x=left;x<right;++x){ZXN_WRITE_REG(0x53,40+(x>>5));for(y=top;y<bottom;++y)h=(h<<1)|(h>>15),h+=*((uint8_t *)(0x6000u+((x&31)<<8)+y));}ZXN_WRITE_REG(0x53,m);return h;}
static void generator(void){
 uint8_t r,i,ok=1;rt_actor a;uint16_t before,t,vm=0,actors=0,render=0;
 check(!init(65),"generator original initialization");agi_vm_set_flag(&rt_vm,5,0);rt_actor_render();shot("build/display-generator-start.l2");
 /* Original keypad button 6: x89..97,y118..131; E6 raises flag128. */
 cmd(0x25,0,90,124);rt_actor_render();before=region(176,200,128,160);
 agi_vm_set_flag(&rt_vm,128,1);r=script(65);rt_actor_render();rt_actor_read(1,&a);
 sprintf(report,"button r=%u x=%u y=%u view=%u cel=%u flags=%u timer=%u\n",r,a.x,a.y,a.view,a.cel,a.flags,rt_vm.vars[87]);log();
 check(!r && rt_vm.vars[31]==1 && a.cel==5 && a.x==88 && a.y==115,"original button selection stays at authored position");
 check(region(176,200,128,160)!=before,"button press changes visible pixels");shot("build/display-generator-key.l2");
 /* Finish the original 6,8,5,8 code through its keypad tests. */
 for(i=0;i<3;++i){cmd(0x25,0,i==1?78:78,i==1?124:140);agi_vm_set_flag(&rt_vm,128,1);r=script(65);rt_actor_render();if(r)ok=0;}
 check(ok && rt_vm.vars[31]==4,"original keypad accepts 6858");
 cmd(0x25,0,80,85);agi_vm_set_flag(&rt_vm,128,1);r=script(65);rt_actor_render();
 check(!r && rt_vm.vars[144]==10,"original code starts detonation");
 for(i=0;i<96 && !r;++i){t=next_clock();r=script(65);vm+=next_clock()-t;t=next_clock();if(!r)r=rt_actor_tick();actors+=next_clock()-t;t=next_clock();if(!r)r=rt_actor_render();render+=next_clock()-t;}
 sprintf(report,"PERF generator scans=%u vm=%u actors=%u render=%u frames\n",i,vm,actors,render);log();shot("build/display-generator-scroll.l2");
 {uint16_t x;uint8_t y,m=ZXN_READ_REG(0x53),white=0;for(x=128;x<192;++x){ZXN_WRITE_REG(0x53,40+(x>>5));for(y=80;y<88;++y)if(*((uint8_t *)(0x6000u+((x&31)<<8)+y))==15)white=1;}ZXN_WRITE_REG(0x53,m);check(!white,"scrolling detonation overwrites all original white dots");}
}
static void balance(void){
 uint8_t i,j,r=0,ok=1;uint16_t outside,t,vm=0,actors=0,render=0;
 check(!init(75),"gambling original initialization");agi_vm_set_flag(&rt_vm,5,0);rt_actor_render();
 outside=region(224,248,160,200);
 for(i=0;i<3;++i){
  rt_vm.vars[124]=i==0?10:i==1?89:190;rt_vm.vars[123]=1;
  cmd(0x24,9,0,0);agi_vm_set_flag(&rt_vm,124,1);r=script(113);
  for(j=1;j<=6;++j)cmd(0x46,j,0,0);
  rt_vm.vars[129]=i==1?3:2;if(!r)r=script(115);rt_actor_render();
  if(r || region(224,248,160,200)!=outside)ok=0;
  if(i==2)shot("build/display-balance.l2");
  /* Original post-result delay, then the cash-in/payout cycle. */
  rt_vm.vars[35]=6;rt_vm.vars[32]=2;rt_vm.vars[33]=2;
  for(j=0;j<16 && !r;++j){t=next_clock();r=script(75);vm+=next_clock()-t;t=next_clock();if(!r)r=rt_actor_tick();actors+=next_clock()-t;t=next_clock();if(!r)r=rt_actor_render();render+=next_clock()-t;if(region(224,248,160,200)!=outside)ok=0;}
 }
 check(!r && ok,"original 1/2/3 digit win-loss balances and payout transitions stay inside panel");shot("build/display-balance-payout.l2");
 sprintf(report,"PERF gambling scans=48 vm=%u actors=%u render=%u frames\n",vm,actors,render);log();
}

extern void rt_text_mask(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
static uint8_t px(uint16_t x,uint8_t y){uint8_t m=ZXN_READ_REG(0x53),c;ZXN_WRITE_REG(0x53,40+(x>>5));c=*((uint8_t *)(0x6000u+((x&31)<<8)+y));ZXN_WRITE_REG(0x53,m);return c;}
static void clipped_text(void){
 uint8_t before[128],x,y,pass,ok=1,expected,m;
 rt.fg=15;rt.bg=0;strcpy(rt_buffer,"Wi");rt_text_at(17,19,rt_buffer,1);
 for(x=0;x<16;++x)for(y=0;y<8;++y)before[x*8+y]=px(152+x,168+y);
 /* Simulate an older snapshot's absent metadata before clipping. */
 m=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,39);memset((void *)0x78a0,0,8);ZXN_WRITE_REG(0x53,m);
 rt_text_mask(4,0,0,0,0);rt_redraw();
 for(x=0;x<16;++x)for(y=0;y<8;++y)if(px(152+x,168+y)!=before[x*8+y])ok=0;
 check(ok,"legacy snapshot reconstructs visible text metadata");
 rt_text_mask(2,79,135,2,8);next_show_rect(76,128,8,8,0);
 rt_text_mask(3,19,17,'W',15);rt_text_mask(3,20,17,'i',15);
 for(pass=0;pass<2;++pass){
  for(x=0;x<16;++x)for(y=0;y<8;++y){expected=x>=6&&x<10?agi_pixel_get((uint16_t)(128+y)*160+76+x/2)&15:before[x*8+y];if(px(152+x,168+y)!=expected)ok=0;}
  if(!pass)rt_redraw();
 }
 check(ok,"partial text occlusion survives repair and full redraw pixel exactly");
}
static void input(void){
 uint8_t m,i,ok=1;char word[41];
 strcpy(path,"games/sq1");rt_init(path);rt.input_enabled=1;
 strcpy(rt.strings[2],"look");cmd(0x75,2,0,0);rt_parser_key('x');cmd(0x77,0,0,0);
 m=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,61);for(i=0;i<40;++i)if(*((uint8_t *)(0x6000u+22*40+i))!=' ')ok=0;ZXN_WRITE_REG(0x53,m);
 check(ok,"prevent.input clears visible command and prompt");
 check(!rt_parser_word(0,word) && !strcmp(word,"look") && agi_vm_flag(&rt_vm,2),"prevent.input preserves parsed words and said eligibility for current scan");
}
static void cel_cache_tests(void){
 next_view v;agi_view_cel a,b;uint8_t r,i,ok=1;uint32_t token;uint16_t before;
 strcpy(path,"build/display-fixtures");resource_cache_init(path);agi_picture_clear();
 r=next_view_open(&v,0);token=v.handle;r|=agi_view_select(&v.source,0,0,&a);r|=agi_view_select(&v.source,0,0,&b);
 check(!r && !memcmp(&a,&b,sizeof(a)) && !a.mirrored,"warm cel metadata cache matches cold selection");
 r=agi_view_blit(&v.source,&a,20,100,8);before=agi_pixel_get(99u*160+20);
 r|=agi_view_blit(&v.source,&a,20,100,8);
 check(!r && before==0x81 && agi_pixel_get(99u*160+20)==before,"warm validated blit matches cold pixels");
 r=agi_view_select(&v.source,1,0,&b);check(!r && b.mirrored && !agi_view_blit(&v.source,&b,30,100,8) && (agi_pixel_get(99u*160+31)&15)==2,"mirrored cel remains distinct in cache");next_view_close(&v);
 for(i=1;i<=2;++i){r=next_view_open(&v,i);r|=agi_view_select(&v.source,0,0,&a);before=agi_pixel_get(99u*160+40);if(r || !agi_view_blit(&v.source,&a,40,100,8) || !agi_view_blit(&v.source,&a,40,100,8) || agi_pixel_get(99u*160+40)!=before)ok=0;next_view_close(&v);}
 check(ok,"malformed and truncated RLE never enter validated cache or paint partially");
 resource_cache_init(path);r=next_view_open(&v,0);r|=agi_view_select(&v.source,0,0,&a);check(!r && v.handle!=token && a.width==4,"runtime reset issues new generation and safely refills cache");next_view_close(&v);
}
int main(void){next_init();next_clock_start();strcpy(path,"build/display-tests.txt");file=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);generator();balance();clipped_text();input();cel_cache_tests();sprintf(report,"SUMMARY checks=%u failures=%u\n",checks,failures);log();esx_f_close(file);for(;;){}}
