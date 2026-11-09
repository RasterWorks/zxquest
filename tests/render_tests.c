#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "agi/picture.h"
static uint8_t file,checks,failures;
static char report[100];
static const uint16_t table[16]={0,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef};
static void check(uint8_t ok,const char *name){++checks;if(!ok)++failures;sprintf(report,"%s %s\n",ok?"PASS":"FAIL",name);esx_f_write(file,report,strlen(report));}
static uint16_t checksum(uint8_t display){
    uint16_t at,crc=0xffff,n,i;uint8_t b,page,saved=ZXN_READ_REG(0x53);
    for(page=display?40:32;page<(display?50:36);++page){
        ZXN_WRITE_REG(0x53,page);n=!display && page==35?2304:8192;
        for(at=0;at<n;at+=i){
            i=n-at;if(i>sizeof(rt_buffer))i=sizeof(rt_buffer);memcpy(rt_buffer,(void *)(0x6000u+at),i);
            for(i=0;i<sizeof(rt_buffer) && at+i<n;++i){b=rt_buffer[i];crc=(crc<<4)^table[(crc>>12)^(b>>4)];crc=(crc<<4)^table[(crc>>12)^(b&15)];}
        }
    }
    ZXN_WRITE_REG(0x53,saved);return crc;
}
static void compare(const char *name){
    uint8_t r,m3=ZXN_READ_REG(0x53),m4=ZXN_READ_REG(0x54);uint16_t picture,display;
    r=rt_actor_render();picture=checksum(0);display=checksum(1);
    rt.dirty|=2;if(!r)r=rt_actor_render();
    check(!r && checksum(0)==picture && checksum(1)==display && ZXN_READ_REG(0x53)==m3 && ZXN_READ_REG(0x54)==m4,name);
}
static uint8_t command(uint8_t op,uint8_t id,uint8_t b,uint8_t c){uint8_t p[7]={0};p[0]=id;p[1]=b;p[2]=c;return rt_command(&rt_vm,op,p);}
static void actor(uint8_t id,uint8_t x,uint8_t y,uint8_t priority){
    command(0x21,id,0,0);command(0x29,id,1,0);command(0x25,id,x,y);command(0x36,id,priority,0);command(0x43,id,0,0);command(0x23,id,0,0);
}
static void glyphs(void){
    static const uint8_t caret[8]={0,16,40,68,0,0,0,0};
    uint8_t c,x,y,col,row,fg,bg,expected,ok=1,saved=ZXN_READ_REG(0x53);uint8_t *dest;const uint8_t *font;
    for(c=32;c<128;++c){
        col=(c-32)%40;row=20+(c-32)/40;fg=c&15;bg=(c+7)&15;
        next_agi_char(col,row,c,fg,bg);if(ZXN_READ_REG(0x53)!=saved)ok=0;
        ZXN_WRITE_REG(0x53,40+(col>>2));dest=(uint8_t *)(0x6020u+((uint16_t)(col&3)<<11)+(uint16_t)row*8);
        font=(const uint8_t *)(0x3d00u+(uint16_t)((c==96?39:c)-32)*8);
        if(c==94)font=caret;
        for(x=0;x<8;++x)for(y=0;y<8;++y){expected=font[y]&(128>>x)?fg:bg;if(dest[(uint16_t)x*256+y]!=expected)ok=0;}
        ZXN_WRITE_REG(0x53,saved);
    }
    check(ok,"all 96 glyphs including AGI apostrophe and caret match reference");
}
static void display_pixels(void){
    uint8_t x,y,p,ok=1,saved=ZXN_READ_REG(0x53);uint8_t *dest;
    for(y=0;y<168;++y)for(x=0;x<160;++x){
        p=agi_pixel_get((uint16_t)y*160+x)&15;ZXN_WRITE_REG(0x53,40+(x>>4));
        dest=(uint8_t *)(0x6000u+((uint16_t)(x&15)<<9)+y+next_picture_top);
        if(dest[0]!=p || dest[256]!=p)ok=0;
    }
    ZXN_WRITE_REG(0x53,saved);check(ok,"rectangle uploads match every packed picture pixel");
}
uint8_t bank_render_tests(void){
    uint8_t r,saved;uint16_t y;rt_actor a;
    failures=checks=0;strcpy(report,"build/render-tests.txt");file=esx_f_open(report,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    strcpy(report,"build/runtime-fixtures");r=rt_init(report);check(!r,"renderer fixtures initialize");
    if(!r){
        agi_picture_clear();next_scene_capture();next_show_picture(0);rt.picture_valid=rt.picture_visible=1;rt.dirty=3;
        actor(0,50,100,8);actor(1,53,105,15);actor(255,145,150,15);compare("initial composition matches full rebuild");
        command(0x25,0,51,101);compare("movement under stationary foreground matches full rebuild");
        command(0x2f,0,1,0);compare("cel change matches full rebuild");
        command(0x2b,0,1,0);compare("mirrored loop change matches full rebuild");
        command(0x36,0,15,0);compare("priority change matches full rebuild");
        command(0x25,0,70,120);compare("leaving overlap restores old pixels");
        command(0x24,1,0,0);compare("erase leaves no stale pixels");
        command(0x23,1,0,0);compare("redraw restores stationary overlapping actor");
        display_pixels();
        saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,48);*((uint8_t *)0x7896)=0xa7;ZXN_WRITE_REG(0x53,saved);
        command(0x25,0,71,120);r=rt_actor_render();ZXN_WRITE_REG(0x53,48);
        check(!r && *((uint8_t *)0x7896)==0xa7,"moving actor preserves unrelated display pixels");*((uint8_t *)0x7896)=15;ZXN_WRITE_REG(0x53,saved);
        rt_actor_reset(1);agi_picture_clear();next_scene_capture();rt.dirty=3;
        actor(0,30,60,15);actor(255,100,150,15);rt_actor_render();
        saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,44);*((uint8_t *)0x6c8c)=0xa7;ZXN_WRITE_REG(0x53,saved);
        command(0x25,0,31,60);command(0x25,255,101,150);r=rt_actor_render();ZXN_WRITE_REG(0x53,44);
        check(!r && *((uint8_t *)0x6c8c)==0xa7,"separated actor bands preserve the untouched vertical gap");ZXN_WRITE_REG(0x53,saved);
        rt_actor_reset(1);agi_picture_clear();
        for(y=0;y<168;++y)agi_pixel_write(y*160+50,0xf0,y<120?0:0x40);
        next_scene_capture();rt.dirty=3;actor(0,48,80,8);actor(1,48,130,15);compare("control-column composition matches full rebuild");
        command(0x25,1,60,130);compare("control-column dependency redraw matches full rebuild");
        /* Replacing a picture must invalidate unchanged actor snapshots. */
        agi_picture_clear();next_scene_capture();rt.dirty=3;compare("new picture recomposes unchanged actors");
        rt_actor_read(0,&a);check(a.old_drawn && a.old_view==a.view && sizeof(a)==32,"actor snapshots fit the existing 32-byte bank stride");
        glyphs();
    }
    sprintf(report,"SUMMARY checks=%u failures=%u\n",(unsigned)checks,(unsigned)failures);esx_f_write(file,report,strlen(report));esx_f_close(file);return !failures;
}
