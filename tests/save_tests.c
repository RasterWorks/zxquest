#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <intrinsic.h>
#include <z80.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "next/save.h"
#include "next/sound.h"
#include "agi/picture.h"
#include "../build/save_manifest.h"
static uint8_t file,checks,failures;
extern uint8_t rt_save_phase,rt_save_errno;
extern uint8_t next_key_decode(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
extern uint8_t rt_save_description(uint8_t,char *);
extern void rt_host_key(uint8_t);
extern void rt_text_mask(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
extern char rt_save_label[32];
static void check(uint8_t ok,const char *name) {
    ++checks;if(!ok)++failures;sprintf(rt_buffer,"%s %s\n",ok?"PASS":"FAIL",name);
    esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_sync(file);
}
static uint8_t init(void) {char path[40];strcpy(path,"build/runtime-fixtures");return rt_init(path);}
static uint8_t scan(void) {uint8_t r;uint16_t n=0;do{r=rt_step(n?0:5);++n;}while(r==VM_RUNNING&&n<500);return r;}
static uint8_t command(uint8_t op,uint8_t a,uint8_t b) {uint8_t p[7]={0};p[0]=a;p[1]=b;return rt_command(&rt_vm,op,p);}
static uint16_t checksum(uint8_t first,uint8_t pages) {
    uint16_t crc=0xffff,at;uint8_t i,b,k,saved=ZXN_READ_REG(0x53);
    for(i=0;i<pages;++i){ZXN_WRITE_REG(0x53,first+i);for(at=0;at<8192;++at){if(first+i==39 && at>=0x1900)continue; /* derived cel-cache tail */
        b=*((uint8_t *)(0x6000u+at));crc^=(uint16_t)b<<8;for(k=0;k<8;++k)crc=(crc&0x8000)?(crc<<1)^0x1021:crc<<1;}}
    ZXN_WRITE_REG(0x53,saved);return crc;
}
static void seed(void) {
    rt_actor a;uint8_t p[7]={2,0,0,70,145,7,0};
    rt.picture_visible=1;rt_picture(0,0);rt.picture_visible=1;
    command(0x21,0,0);command(0x29,0,1);rt_actor_read(0,&a);a.x=50;a.y=140;a.flags|=ACT_DRAWN;a.old_drawn=0;rt_actor_write(0,&a);
    rt_actor_command(0x7a,p);rt_actor_render();rt_redraw();
    rt.fg=15;rt.bg=0;strcpy(rt_buffer,"Overlay");rt_text_at(5,4,rt_buffer,1);rt_text_mask(2,16,39,2,8);rt_redraw();
    rt.input_enabled=1;rt.status_enabled=1;rt_vm.vars[7]=255;rt_parser_reset_input();rt_parser_cycle(0);
    rt_vm.vars[3]=37;rt_vm.vars[200]=123;agi_vm_set_flag(&rt_vm,211,1);
    rt.locations[0]=255;rt.locations[1]=7;strcpy(rt.strings[1],"Saved string");
    rt_vm.scan[100]=1234;rt.random=0x9876;rt.horizon=42;rt.block_on=1;memset(rt.block,10,4);
}
static void corrupt_copy(const char *destination,uint16_t offset) {
    char source[64],target[64];uint8_t in,out;uint16_t n;uint32_t at=0;volatile uint8_t *b=(uint8_t *)rt_buffer;
    strcpy(source,SAVE_ROOT "/1.a");strcpy(target,destination);
    in=esx_f_open(source,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);
    out=esx_f_open(target,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    while((n=esx_f_read(in,rt_buffer,512))!=0) {
        if(at<=offset && offset-at<n)b[(uint16_t)(offset-at)]=b[(uint16_t)(offset-at)]^1;
        esx_f_write(out,rt_buffer,n);at+=n;
    }
    esx_f_close(in);esx_f_close(out);
}
static void roundtrip(void) {
    uint8_t r,held[15],b,f;uint16_t composite,background;rt_actor a;char path[64];
    check(!init(),"fixture initialized");seed();composite=checksum(32,4);background=checksum(36,4);
    r=rt_save_slot(1,0);
    sprintf(rt_buffer,"SAVE result=%u phase=%u errno=%u\n",(unsigned)r,(unsigned)rt_save_phase,(unsigned)rt_save_errno);esx_f_write(file,rt_buffer,strlen(rt_buffer));
    check(!r,"save written, verified and committed");if(r)return;
    rt_vm.vars[200]=99;rt.locations[0]=0;rt.strings[1][0]=0;rt_vm.scan[100]=0;agi_picture_clear();
    r=rt_save_slot(1,1);check(!r,"restore succeeds");if(r)return;
    check(rt_vm.vars[200]==123 && rt_vm.vars[3]==37 && agi_vm_flag(&rt_vm,211),"variables, score and flags restored");
    check(rt.locations[0]==255 && rt.locations[1]==7 && !strcmp(rt.strings[1],"Saved string"),"inventory locations and strings restored");
    rt_actor_read(0,&a);check(a.x==50 && a.y==140 && a.view==1,"actor state restored");
    check(rt.random!=0x9876 && rt.horizon==42 && rt.block_on && rt.block[3]==10 && rt_vm.scan[100]==1234,"random future refreshed while collision block and scan starts restore");
    check(checksum(32,4)==composite && checksum(36,4)==background,"composite and permanent stamped background restored exactly");
    check(agi_vm_flag(&rt_vm,12) && !rt.ui && rt_vm.status==VM_DONE && !rt_vm.depth,"restore exits old VM and sets flag 12");
    check(!scan() && rt_vm.vars[221]==1 && !agi_vm_flag(&rt_vm,12),"restored game executes LOGIC 0 once with flag 12");
    check(!init() && !rt_save_slot(1,1) && rt_vm.vars[200]==123,"save survives full runtime reinitialization");
    check(!resource_scratch(15,held),"reserve cache pages for low-memory test");
    rt_vm.vars[200]=42;r=rt_save_slot(1,1);
    check(r==SAVE_MEMORY && rt_vm.vars[200]==42,"low-memory restore leaves live game unchanged");resource_scratch_release(15,held);
    corrupt_copy(SAVE_ROOT "/3.a",70);
    r=rt_save_slot(3,1);check(r==SAVE_INVALID && rt_vm.vars[200]==42,"checksum rejects damaged payload before mutation");
    check(!rt_save_slot(2,0),"second slot saved independently");
    rt_vm.vars[200]=77;check(!rt_save_slot(2,0),"replacement save succeeds");
    strcpy(path,SAVE_ROOT "/2.b");f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);esx_f_write(f,"short",5);esx_f_close(f);
    r=rt_save_slot(2,1);check(!r && rt_vm.vars[200]==42,"previous generation recovers from interrupted replacement");
    strcpy(path,SAVE_ROOT "/3.a");f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);esx_f_write(f,"short",5);esx_f_close(f);
    r=rt_save_slot(3,1);check(r==SAVE_INVALID && rt_vm.vars[200]==42,"truncated save rejected without changing state");
    corrupt_copy(SAVE_ROOT "/7.a",16);
    check(rt_save_slot(7,1)==SAVE_GAME && rt_vm.vars[200]==42,"save from different game rejected before mutation");
    strcpy(path,SAVE_ROOT "/4.a");esx_f_unlink(path);strcpy(path,SAVE_ROOT "/4.b");esx_f_unlink(path);
    check(rt_save_slot(4,1)==SAVE_EMPTY,"empty slot handled without halting VM");
    check(rt_save_slot(0,0)==SAVE_INVALID && rt_save_slot(10,1)==SAVE_INVALID,"invalid slot numbers rejected");
}
static void random_restore(void) {
    uint8_t first[24],p[7]={0,3,200},i,different=0,r;uint16_t state;
    r=rt_save_slot(1,1);
    for(i=0;i<24;++i){rt_command(&rt_vm,0x82,p);first[i]=rt_vm.vars[200];}
    r|=rt_save_slot(1,1);
    for(i=0;i<24;++i){rt_command(&rt_vm,0x82,p);if(first[i]!=rt_vm.vars[200])different=1;}
    check(!r && different,"restoring the same snapshot does not replay its next 24 reel symbols");
    state=rt.random;r=rt_save_slot(4,1);
    check(r==SAVE_EMPTY && rt.random==state,"failed restore preserves the live random stream");
}
static void modal(void) {
    uint8_t r,i;uint16_t screen;char description[32];
    init();seed();screen=checksum(40,10);rt.pending[100>>3]|=1u<<(100&7);r=scan();
    check(r==VM_WAIT && rt.ui==RT_UI_SAVE,"save.game opcode suspends in slot dialog");
    rt_key(27,0);check(!rt.ui && checksum(40,10)==screen,"cancel restores exact modal backing");
    check(!scan() && rt_vm.vars[222]==1,"cancel resumes instruction following save.game");
    screen=checksum(40,10); /* Resumed scan can refresh the score/status line. */
    rt.pending[100>>3]|=1u<<(100&7);r=scan();
    for(i=0;i<4;++i)rt_host_key(10);
    rt_key(13,0);
    check(rt.ui==RT_UI_SAVE && rt_vm.vars[222]==1,"first Enter awaits save confirmation without resuming game");
    rt_key('A',0);rt_key('i',0);rt_key('r',0);rt_key('x',0);rt_key(8,0);
    check(!strcmp(rt_save_label,"Air"),"save label edits characters and backspace before writing");
    rt_key(13,0);
    check(rt.ui==RT_UI_SAVE,"description Enter opens final save confirmation");
    rt_key(13,0);
    check(!rt_save_description(5,description) && !strcmp(description,"Air") && !rt_save_label[0],"custom label persists in compatible header and transient label clears");
    check(!rt.ui && !rt.error && checksum(40,10)==screen,"confirmation Enter saves and restores exact game screen");
    check(!scan() && rt_vm.vars[222]==2,"confirmed save resumes suspended VM without another key");
    rt_vm.vars[200]=77;rt.pending[101>>3]|=1u<<(101&7);r=scan();
    check(r==VM_WAIT && rt.ui==RT_UI_SAVE,"restore.game opcode opens slot dialog");
    rt_key('5',0);rt_key(13,0);rt_key(13,0);
    check(!rt.ui && rt_vm.vars[200]==123 && !rt_vm.vars[223] && agi_vm_flag(&rt_vm,12),"restore abandons old continuation and restores saved state");
    check(!scan() && rt_vm.vars[221]==1 && !rt_vm.vars[223],"restore starts fresh LOGIC 0 without replaying restore opcode");
    screen=checksum(40,10);rt.pending[101>>3]|=1u<<(101&7);r=scan();rt_key('4',0);rt_key(13,0);rt_key(13,0);
    check(rt.ui==RT_UI_SAVE && !rt.error && rt_vm.vars[200]==123,"failed restore keeps error dialog and live game state");
    rt_key(13,0);
    check(!rt.ui && checksum(40,10)==screen && !scan() && rt_vm.vars[223]==1,"dismissing error restores game screen and resumes VM");
    command(0x63,1,212);r=rt_save_slot(6,0);rt_sound_stop();r|=rt_save_slot(6,1);
    check(!r && !rt.sound_active && agi_vm_flag(&rt_vm,212),"restoring active sound completes its flag without replay");
}
static void keyboard(void) {
    uint8_t i,r=1,iff=z80_get_int_state();
    intrinsic_di();
    next_key_decode(0,0,0,255,255,0);
    for(i=0;i<5;++i) {
        if(next_key_decode(0,0,1,255^(1<<i),255,0)!=128+i)r=0;
        next_key_decode(0,0,0,255,255,0);
        if(next_key_decode(0,0,0,255,255^(1<<i),1)!=137-i)r=0;
        next_key_decode(0,0,0,255,255,0);
    }
    check(r,"Extend plus all ten digits maps to F1-F10 with classic modifier fallback");
    r=next_key_decode('5',0,1,239,255,0);
    check(r==132 && next_key_decode('5',0,0,239,255,0)==132 && !next_key_decode(0,0,0,255,255,0),"releasing Extend first does not leak digit into command line");
    check(next_key_decode(0,1,0,255,255,0)==147 && next_key_decode(0,2,0,255,255,0)==167 && next_key_decode(0,4,0,255,255,0)==165 && next_key_decode(0,8,0,255,255,0)==161,"four dedicated arrows use existing movement/menu events");
    check(next_key_decode('q',255,255,255,255,0)=='q' && next_key_decode('!',0,0,255,255,0)=='!',"older emulator register fallback and Symbol Shift punctuation survive");
    check(next_key_decode(0,0,4,255,255,0)==0 && next_key_decode(0,0,32,255,255,0)==7 && next_key_decode(0,0,128,255,255,0)==12,"Graph is inactive; Break and Delete decode correctly");
    next_key_decode(0,0,0,255,255,1);next_key_decode(0,0,0,255,255,0);
    check(next_key_decode('5',0,0,239,255,0)==132,"CSpect Tab prefix survives modifier release before number arrives");
    next_key_decode(0,0,0,255,255,0);
    check(next_key_decode('5',0,0,239,255,0)=='5',"function prefix is consumed and normal number typing resumes");
    /* Q/J/I are assigned shortcuts; use an unassigned letter for cancellation. */
    next_key_decode(0,0,1,255,255,0);next_key_decode('x',0,0,255,255,0);
    next_key_decode(0,0,0,255,255,0);
    check(next_key_decode('5',0,0,239,255,0)=='5',"non-number key cancels one-shot function prefix");
    z80_set_int_state(iff);
}
uint8_t bank_save_tests(void) {
    char path[40];strcpy(path,"build/save-tests.txt");file=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    checks=failures=0;
    {
        char to[40];uint8_t f,r,e;
        strcpy(path,"build/mkdir-probe");errno=0;r=esx_f_mkdir(path);e=errno;
        sprintf(rt_buffer,"MKDIR result=%u errno=%u\n",(unsigned)r,(unsigned)e);esx_f_write(file,rt_buffer,strlen(rt_buffer));
        strcpy(path,"build/rename-probe.a");errno=0;f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
        esx_f_write(f,"x",1);errno=0;r=esx_f_sync(f);e=errno;esx_f_close(f);
        sprintf(rt_buffer,"SYNC result=%u errno=%u\n",(unsigned)r,(unsigned)e);esx_f_write(file,rt_buffer,strlen(rt_buffer));
        strcpy(to,"build/rename-probe.b");errno=0;r=esx_f_rename(path,to);e=errno;
        sprintf(rt_buffer,"RENAME result=%u errno=%u\n",(unsigned)r,(unsigned)e);esx_f_write(file,rt_buffer,strlen(rt_buffer));
        errno=0;r=esx_f_unlink(to);e=errno;
        sprintf(rt_buffer,"UNLINK result=%u errno=%u\n",(unsigned)r,(unsigned)e);esx_f_write(file,rt_buffer,strlen(rt_buffer));
    }
    keyboard();roundtrip();random_restore();modal();
    sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",(unsigned)checks,(unsigned)failures);esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_close(file);return !failures;
}
