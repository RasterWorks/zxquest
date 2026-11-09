#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <intrinsic.h>
#include <z80.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "next/sound.h"
static uint8_t file,checks,failures;
extern void rt_host_key(uint8_t);
extern void rt_key_labels(char *,uint16_t);
static void check(uint8_t ok,const char *name){++checks;if(!ok)++failures;sprintf(rt_buffer,"%s %s\n",ok?"PASS":"FAIL",name);esx_f_write(file,rt_buffer,strlen(rt_buffer));}
static uint16_t checksum(void){uint8_t p,m=ZXN_READ_REG(0x53);uint16_t at,n=0;for(p=40;p<50;++p){ZXN_WRITE_REG(0x53,p);for(at=0;at<8192;++at)n=(n<<1)|(n>>15),n+=*((uint8_t *)(0x6000u+at));}ZXN_WRITE_REG(0x53,m);return n;}
static void snapshot(const char *name){uint8_t f,p,m=ZXN_READ_REG(0x53);strcpy(rt_buffer,name);f=esx_f_open(rt_buffer,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);for(p=40;p<50;++p){ZXN_WRITE_REG(0x53,p);esx_f_write(f,(void *)0x6000,8192);}esx_f_close(f);ZXN_WRITE_REG(0x53,m);}
static uint8_t init(void){char path[40];strcpy(path,"build/runtime-fixtures");return rt_init(path);}
static uint8_t command(uint8_t op,uint8_t a){uint8_t p[7]={0};p[0]=a;return rt_command(&rt_vm,op,p);}
static uint8_t scan(void){uint16_t n=0;uint8_t r;do{r=rt_step(n?0:5);++n;}while(r==VM_RUNNING && n<500);return r;}
static uint8_t settle(void){uint8_t r=scan(),n=0;while(r==VM_WAIT && rt.ui==RT_UI_MESSAGE && ++n<20){rt_key(13,0);r=scan();}return r;}
static void host_controls(void){
    uint16_t before,ticks;uint8_t mode,r,i;
    check(!init(),"fixture initializes");before=checksum();
    next_agi_char(3,3,39,0,15);before=checksum();next_agi_char(3,3,96,0,15);
    check(checksum()==before,"AGI backtick draws apostrophe rather than Spectrum pound glyph");
    check(command(0x86,0)==VM_WAIT && rt.ui==RT_UI_CONTROL,"quit opens confirmation");
    snapshot("build/control-quit.l2");
    rt_key('x',0);check(rt.ui==RT_UI_CONTROL && !rt_control_action,"quit ignores unrelated keys");
    rt_key(27,0);check(!rt.ui && !rt_control_action && checksum()==before,"quit cancellation restores exact scene");
    command(0x86,0);rt_key(13,0);check(rt_control_action==1 && rt_step(0)==VM_WAIT && !sound_playing && !rt_vm.depth,"confirmed quit stops VM and sound before platform exit");
    init();check(command(0x86,1)==VM_WAIT && rt_control_action==1,"quit nonzero parameter omits confirmation");
    init();rt_vm.vars[200]=42;command(0x80,0);rt_key(27,0);check(rt_vm.vars[200]==42 && !rt_control_action,"restart cancellation preserves game");
    agi_vm_set_flag(&rt_vm,9,0);command(0x80,0);rt_key(13,0);r=rt_step(0);
    check(!r && !rt_vm.vars[200] && agi_vm_flag(&rt_vm,6) && !agi_vm_flag(&rt_vm,9) && !rt_control_action,"restart resets same game, sets flag 6 and preserves sound choice");
    agi_vm_set_flag(&rt_vm,16,1);check(command(0x80,0)==VM_WAIT && rt_control_action==2 && !rt.ui,"automatic restart flag suppresses confirmation");rt_step(0);
    agi_vm_set_flag(&rt_vm,9,1);check(!rt_sound_start(1,210) && sound_playing,"pause test starts a live sound stream");ticks=sound_ticks;before=checksum();
    r=command(0x88,0);mode=rt_vm.vars[11];rt_step(200);
    snapshot("build/control-pause.l2");
    check(r==VM_WAIT && rt_paused && rt_vm.vars[11]==mode && sound_ticks==ticks,"pause freezes game clock and audio stream");
    check(!sound_registers[0][8]&&!sound_registers[0][9]&&!sound_registers[0][10]&&!sound_registers[1][8],"paused AY channels are silent");
    rt_key(0,0x3b);check(!rt.ui && !rt_paused && checksum()==before,"pause dismissal restores framebuffer without dispatching key");rt_sound_stop();
    strcpy(rt_buffer,"F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 Ctrl-J Alt-Z <Tab>");rt_key_labels(rt_buffer,sizeof(rt_buffer));
    check(!strcmp(rt_buffer,"E1 E2 E3 E4 E5 E6 E7 E8 E9 E0 E-J E-Q <INV>"),"all function and letter labels use Next hardware names");
    rt.key_count=13;memset(rt.pending,0,32);
    for(i=0;i<10;++i){rt.keys[i].ascii=0;rt.keys[i].scan=0x3b+i;rt.keys[i].controller=i;rt_host_key(128+i);}
    rt.keys[10].ascii=0;rt.keys[10].scan=0x2c;rt.keys[10].controller=10;rt_host_key(139);
    rt.keys[11].ascii=10;rt.keys[11].scan=0;rt.keys[11].controller=11;rt_host_key(140);
    rt.keys[12].ascii=9;rt.keys[12].scan=0;rt.keys[12].controller=12;rt_host_key(138);
    check(rt.pending[0]==255 && rt.pending[1]==31,"F1-F10, Quit, Joystick and Edit dispatch original AGI bindings");
}
static void joystick(void){
    uint8_t reg=ZXN_READ_REG(5),i,ok=1,writable;
    static const uint8_t raw[8]={8,9,1,5,4,6,2,10};
    init();rt.player_control=1;rt.gfx=1;
    ZXN_WRITE_REG(5,reg^2);writable=ZXN_READ_REG(5)==(reg^2);ZXN_WRITE_REG(5,reg);
    if(!writable){strcpy(rt_buffer,"NOTE emulator does not echo joystick-mode writes; connector routing requires physical Next validation\n");esx_f_write(file,rt_buffer,strlen(rt_buffer));}
    command(0x8b,0);snapshot("build/control-joystick.l2");rt_key(0,0x50);rt_key(27,0);check(!rt_joystick_mode && ZXN_READ_REG(5)==reg,"joystick cancel preserves hardware configuration");
    command(0x8b,0);rt_key(0,0x50);rt_key(13,0);
    check(rt_joystick_mode==1 && (ZXN_READ_REG(5)&0xc8)==0x40 && (ZXN_READ_REG(5)&0x37)==(reg&0x37),"port 1 setup preserves video and port 2 settings");
    for(i=0;i<8;++i){rt_joystick(raw[i]);if(rt_vm.vars[6]!=i+1)ok=0;rt_joystick(0);if(rt_vm.vars[6])ok=0;}
    check(ok,"all eight joystick directions stop on release");
    rt_vm.vars[6]=3;rt_joystick(0);check(rt_vm.vars[6]==3,"idle joystick leaves keyboard walking alone");
    rt.player_control=0;rt_joystick(2);check(rt_vm.vars[6]==3,"joystick does not override scripted movement");rt.player_control=1;rt_joystick(0);
    rt_joystick(255);check(rt_vm.vars[6]==3,"absent interface does not move or press buttons");
    command(0x8b,0);rt_joystick(4);rt_joystick(0);rt_joystick(16);
    sprintf(rt_buffer,"JOY mode=%u ui=%u reg=%02X error=%u\n",rt_joystick_mode,rt.ui,ZXN_READ_REG(5),rt.error);esx_f_write(file,rt_buffer,strlen(rt_buffer));
    check(!rt.ui && rt_joystick_mode==2 && (!writable || (ZXN_READ_REG(5)&0x32)==2),"joystick navigates setup and Fire confirms port 2");
    rt_joystick(0);command(0x86,0);rt_joystick(32);check(!rt.ui && !rt_control_action,"Fire 2 cancels quit");
    rt_joystick_config(0);check(ZXN_READ_REG(5)==reg,"keyboard-only mode restores original joystick configuration");
}
static void original(void){
    char path[40];uint16_t n;uint8_t r,sound,i;
    strcpy(path,"games/sq1");check(!rt_init(path),"original SQ1 initializes");
    for(n=0;n<3000;++n){r=scan();if(r==VM_WAIT){if(rt.ui==RT_UI_STRING)rt_key('A',0);rt_key(13,0);continue;}if(r)break;if(n && rt.player_control && rt.input_enabled && rt.picture_visible)break;if(n%10==0)rt_key(' ',0);}
    check(n<3000 && !r && rt.player_control,"original SQ1 reaches gameplay");
    sound=agi_vm_flag(&rt_vm,9);rt_host_key(129);settle();check(agi_vm_flag(&rt_vm,9)!=sound,"E2 toggles original sound flag");rt_host_key(129);settle();
    rt_host_key(139);r=scan();check(r==VM_WAIT && rt.ui==RT_UI_CONTROL,"EQ reaches original Quit command");rt_key(27,0);settle();
    rt_host_key(140);r=scan();
    sprintf(rt_buffer,"EXTJ r=%u ui=%u logic=%u pc=%u op=%02X\n",r,rt.ui,rt_vm.logic,rt_vm.pc,rt_vm.opcode);esx_f_write(file,rt_buffer,strlen(rt_buffer));
    for(i=0;i<rt.key_count;++i){sprintf(rt_buffer,"KEY %u %u controller=%u\n",rt.keys[i].ascii,rt.keys[i].scan,rt.keys[i].controller);esx_f_write(file,rt_buffer,strlen(rt_buffer));}
    check(r==VM_WAIT && rt.ui==RT_UI_CONTROL,"EJ reaches original joystick command");rt_key(27,0);settle();
    rt_host_key(138);r=scan();check(r==VM_WAIT && rt.ui==RT_UI_INVENTORY,"INV VIDEO opens original inventory");rt_key(27,0);settle();
    rt_host_key(128);r=scan();check(!rt.gfx,"E1 opens original Help");rt_key(27,0);r=scan();check(rt.gfx && !rt.ui,"Help dismissal consumes Escape");
    rt_host_key(136);r=scan();check(r==VM_WAIT && rt.ui==RT_UI_CONTROL,"E9 reaches original restart command");rt_key(27,0);settle();
    for(i=0;i<rt.item_count;++i){rt_format(rt.items[i].logic,rt.items[i].message,rt_buffer,sizeof(rt_buffer));if(strstr(rt_buffer,"Pause"))break;}
    if(i<rt.item_count){rt_host_key(27);r=scan();sound=rt_vm.vars[11];rt_step(200);check(r==VM_WAIT && rt.ui==RT_UI_MENU && rt_vm.vars[11]==sound,"original SQ1 menu pauses the game clock");rt.menu_selected=rt.items[i].menu;rt.item_selected=i;rt_key(13,0);r=scan();}else r=VM_ARGUMENT;
    sprintf(rt_buffer,"PAUSE r=%u ui=%u paused=%u item=%u count=%u logic=%u pc=%u op=%02X\n",r,rt.ui,rt_paused,i,rt.item_count,rt_vm.logic,rt_vm.pc,rt_vm.opcode);esx_f_write(file,rt_buffer,strlen(rt_buffer));
    check(r==VM_WAIT && rt.ui==RT_UI_CONTROL && rt_paused,"original menu Pause reaches host pause command");rt_key(27,0);settle();
}

extern uint8_t next_key_decode(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
extern void next_key_reset(void);
extern void next_key_sample(uint8_t);
extern uint8_t next_key(void);
static uint8_t decode(uint8_t key,uint8_t arrows,uint8_t extra,uint8_t left,uint8_t right){return next_key_decode(key,arrows,extra,left,right,0);}
static void keyboard(void){
    static const uint8_t masks[8]={8,9,1,5,4,6,2,10};
    uint8_t i,k,ok=1,iff=z80_get_int_state();intrinsic_di();next_key_reset();decode(0,0,0,255,255);
    for(i=0;i<5;++i){
        if(decode('1'+i,0,64,255^(1<<i),255)!=128+i)ok=0;
        decode(0,0,0,255,255);
        if(decode(0,0,64,255,255^(1<<i))!=137-i)ok=0;
        decode(0,0,0,255,255);
    }
    check(ok,"held EDIT with all ten digits emits F1-F10");
    check(!decode(0,0,64,255,255) && !decode(0,0,0,255,255) && decode('5',0,0,239,255)=='5',"EDIT tap is only a modifier and never arms a later digit");
    check(decode('5',0,64,239,255)==132 && decode('5',0,0,239,255)==132,"releasing EDIT first never leaks its function digit");decode(0,0,0,255,255);
    check(decode('q',0,64,255,255)==139,"EDIT Q emits Quit");decode(0,0,0,255,255);
    check(decode('j',0,64,255,255)==140,"EDIT J emits joystick selection");decode(0,0,0,255,255);
    check(decode(0,0,16,255,255)==138 && decode(0,0,32,255,255)==7,"INV VIDEO and BREAK emit inventory and cancel/menu");decode(0,0,0,255,255);
    decode('5',0,64,239,255);check(decode('5',0,32,239,255)==7,"BREAK overrides a held function key");decode(0,0,0,255,255);
    ok=1;for(i=0;i<8;++i){k=decode(0,masks[i],0,255,255);if(k!=145+i)ok=0;decode(0,0,0,255,255);}
    check(ok,"all eight physical arrow directions decode correctly");
    k=decode(0,1,0,255,255);check(k==147 && decode(0,9,0,255,255)==162 && decode(0,8,0,255,255)==162 && !decode(0,0,0,255,255),"staggered diagonal presses refine direction and releases retain it");
    check(decode(0,3,0,255,255)==160,"opposing horizontal arrows cancel");decode(0,0,0,255,255);
    check(decode(0,12,0,255,255)==160,"opposing vertical arrows cancel");decode(0,0,0,255,255);
    check(decode(0,7,0,255,255)==149,"opposing component cancels while remaining axis moves");decode(0,0,0,255,255);
    next_key_sample(132);next_key_reset();check(!next_key() && !decode('5',0,64,239,255),"restart flushes queue and waits for held keys to release");intrinsic_di();decode(0,0,0,255,255);
    check(decode(0,0,32,255,255)==7,"BREAK works after decoder reset");decode(0,0,0,255,255);z80_set_int_state(iff);
    init();rt.player_control=1;rt.gfx=1;rt_host_key(147);rt_host_key(162);check(rt_vm.vars[6]==2,"a second arrow sets diagonal without toggling off");rt_host_key(146);check(!rt_vm.vars[6],"pressing the chosen diagonal again stops walking");
    rt_host_key(148);check(rt_vm.vars[6]==4,"southeast movement reaches actor direction");rt_host_key(160);check(!rt_vm.vars[6],"opposing arrows stop movement");
}
static void restart_menu(void){
    uint8_t r,i,round,menus=rt.menu_count,items=rt.item_count;
    for(round=0;round<2;++round){
        rt_host_key(136);r=scan();rt_key(13,0);r=rt_step(0);
        check(!r && rt.menus_ready && rt.menu_count==menus && rt.item_count==items,"restart retains submitted menu definitions");
        for(i=0;i<40 && !rt.input_enabled;++i){r=settle();if(r)break;}
        rt_host_key(7);r=scan();check(r==VM_WAIT && rt.ui==RT_UI_MENU,"BREAK opens original SQ1 menu after restart");
        rt_host_key(149);check(rt.item_selected<rt.item_count,"down arrow navigates retained menu");
        rt_host_key(7);r=settle();check(!r && !rt.ui,"BREAK closes restarted menu without another key");
        rt_host_key(132);r=scan();check(r==VM_WAIT && rt.ui==RT_UI_SAVE,"E5 works after restart");rt_key(27,0);settle();
        rt_host_key(138);r=scan();check(r==VM_WAIT && rt.ui==RT_UI_INVENTORY,"inventory works after restart");rt_key(27,0);settle();
    }
}
int main(void){
    next_init();next_sound_manual=1;next_clock_start();
    strcpy(rt_buffer,"build/control-tests.txt");file=esx_f_open(rt_buffer,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    keyboard();host_controls();joystick();original();restart_menu();
    sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",(unsigned)checks,(unsigned)failures);esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_close(file);for(;;){}
}
