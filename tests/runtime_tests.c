#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <intrinsic.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "agi/picture.h"
#include "../build/runtime_manifest.h"
static uint16_t checks,failures;
static uint8_t log_file,params[7];
/* Logging finishes before the next interpreter operation uses its scratch. */
#define report rt_buffer
static char directory[32];
static rt_actor a;
static resource_cache_info info;
extern uint8_t runtime_feedback_tests(void);
extern uint8_t parser_tests(void);
static void log_line(const char *s) { uint16_t n=strlen(s); if(esx_f_write(log_file,(void *)s,n)!=n) for(;;){} }
static void check(uint8_t ok,const char *name) { ++checks; if(!ok) ++failures; sprintf(report,"%s %s\n",ok?"PASS":"FAIL",name); log_line(report); }
static uint8_t command(uint8_t op,uint8_t x,uint8_t y,uint8_t z,uint8_t q,uint8_t w) {
    params[0]=x;params[1]=y;params[2]=z;params[3]=q;params[4]=w;
    return rt_command(&rt_vm,op,params);
}
static uint8_t scan(void) {
    uint8_t r; uint16_t n=0,before=rt.cycles;
    do { r=rt_step(n ? 0 : 5); ++n; } while(r==VM_RUNNING && n<500);
    if(n==500 || (r==VM_DONE && rt.cycles==before)) return VM_ARGUMENT;
    return r;
}
static uint8_t script(uint8_t id) {
    uint8_t r=agi_vm_start(&rt_vm,id); uint16_t n=0;
    while(r==VM_RUNNING && n++<500) r=agi_vm_run(&rt_vm,40);
    return r;
}
static uint16_t checksum(uint8_t clean) {
    uint16_t crc=0xffff,at; uint8_t b,i,saved=ZXN_READ_REG(0x53),page=255;
    for(at=0;at<AGI_PIXELS;++at) {
        if(clean) { if((at>>13)!=page){page=at>>13;ZXN_WRITE_REG(0x53,36+page);} b=*((uint8_t *)(0x6000u+(at&8191))); }
        else b=agi_pixel_get(at);
        crc^=(uint16_t)b<<8; for(i=0;i<8;++i) crc=(crc&0x8000) ? (crc<<1)^0x1021 : crc<<1;
    }
    ZXN_WRITE_REG(0x53,saved); return crc;
}
static void dump(const char *path) {
    uint8_t f; uint16_t at,i,n;
    errno=0;f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    if(errno){++failures;return;}
    for(at=0;at<AGI_PIXELS;at+=n){n=AGI_PIXELS-at;if(n>sizeof(rt_buffer))n=sizeof(rt_buffer);for(i=0;i<n;++i)rt_buffer[i]=agi_pixel_get(at+i);if(esx_f_write(f,rt_buffer,n)!=n)++failures;}
    esx_f_close(f);
}
static void authored(void) {
    uint8_t r,value,saved2,saved3,saved4; uint16_t crc,i;
    saved2=ZXN_READ_REG(0x52);saved3=ZXN_READ_REG(0x53);saved4=ZXN_READ_REG(0x54);
    strcpy(directory,"build/runtime-fixtures");
    check(!rt_init(directory) && rt.objects==2 && rt.locations[0]==255 && rt.locations[1]==7,
          "encrypted OBJECT initializes inventory locations");
    check(sizeof(rt_actor)<=32 && sizeof(rt_vm)==1067,"resident VM and banked actor record bounds");
    check(!scan() && !strcmp(rt.game_id,"TEST") && rt.cycles==1 && rt.picture_visible && rt.key_count==2,
          "script initializes room graphics, bindings and interpreter state");
    check(ZXN_READ_REG(0x54)==saved4,"nested VM-command-picture-VIEW gates restore code bank");
    rt_actor_read(0,&a); check(a.x==50 && a.y==140 && a.view==1 && (a.flags&ACT_DRAWN),"script positions and draws actor");
    crc=checksum(1);
    command(0x24,0,0,0,0,0);r=rt_actor_render();
    check(!r && checksum(0)==crc,"erase restores complete original picture without trails");
    command(0x21,255,0,0,0,0);command(0x29,255,2,0,0,0);command(0x25,255,70,140,0,0);
    command(0x36,255,15,0,0,0);command(0x43,255,0,0,0,0);command(0x23,255,0,0,0,0);
    command(0x21,1,0,0,0,0);command(0x29,1,3,0,0,0);command(0x25,1,70,140,0,0);
    command(0x36,1,4,0,0,0);command(0x23,1,0,0,0,0);r=rt_actor_render();
    check(!r && (agi_pixel_get(140u*160+70)&15)==2,"all 256 actor IDs, priority order independent of object ID");
    command(0x36,1,15,0,0,0);rt_actor_render();
    check((agi_pixel_get(140u*160+70)&15)==2,"equal-priority ordering is stable by object ID");
    command(0x24,255,0,0,0,0);command(0x24,1,0,0,0,0);rt_actor_render();
    check(checksum(0)==crc,"overlapping sprite erase restores clean background");
    command(0x23,1,0,0,0,0);command(0x23,255,0,0,0,0);rt_actor_render();
    command(0x22,0,0,0,0,0);rt_actor_render();rt_actor_read(255,&a);
    check(checksum(0)==crc&&!(a.flags&(ACT_DRAWN|ACT_ANIMATED)),"unanimate.all preserves old rectangles for cleanup");
    command(0x21,2,0,0,0,0);command(0x29,2,1,0,0,0);command(0x25,2,40,140,0,0);command(0x23,2,0,0,0,0);
    command(0x49,2,220,0,0,0);
    for(i=0;i<40 && !agi_vm_flag(&rt_vm,220);++i) if(rt_actor_tick()) break;
    check(agi_vm_flag(&rt_vm,220),"end.of.loop sets completion flag and stops cycling");
    command(0x4b,2,221,0,0,0);
    for(i=0;i<40 && !agi_vm_flag(&rt_vm,221);++i) if(rt_actor_tick()) break;
    rt_actor_read(2,&a);check(agi_vm_flag(&rt_vm,221)&&!a.cel,"reverse.loop returns to first cel");
    command(0x51,2,45,140,3,222);
    for(i=0;i<10 && !agi_vm_flag(&rt_vm,222);++i) if(rt_actor_tick()) break;
    rt_actor_read(2,&a);check(agi_vm_flag(&rt_vm,222)&&a.x==45&&a.step==1,"move.obj arrives and restores original step size");
    rt_key('a',0);r=scan();check(!r&&rt_vm.vars[100]==1&&agi_vm_flag(&rt_vm,210),"key binding triggers controller and have.key for one scan");
    r=scan();check(!r&&rt_vm.vars[100]==1,"controller cleared after scan");
    rt_key('b',0);r=scan();check(!r&&rt_vm.vars[100]==2,"second key can bind to same controller");
    strcpy(rt.strings[1],"Roger");rt_vm.vars[3]=12;
    check(!rt_format(1,1,rt_buffer,sizeof(rt_buffer))&&!strcmp(rt_buffer,"Hello Roger. Score 12."),"message substitutes strings and variables");
    r=script(2);resource_cache_status(&info);
    check(r==VM_WAIT&&rt.ui==RT_UI_MESSAGE&&rt_vm.depth==2&&info.pinned==2&&!rt_vm.vars[200],"modal print suspends nested call with pins intact");
    check(agi_vm_run(&rt_vm,100)==VM_WAIT&&!rt_vm.vars[200],"waiting VM cannot advance without host resume");
    rt_key(13,0);agi_vm_resume(&rt_vm);r=agi_vm_run(&rt_vm,100);resource_cache_status(&info);
    check(!r&&rt_vm.vars[200]==17&&rt_vm.vars[201]==1&&!info.pinned,"Enter resumes after print and nested calls release pins");
    r=script(3);check(r==VM_WAIT&&rt.ui==RT_UI_STRING&&!rt_vm.vars[202],"get.string waits for typed text");
    rt_key('a',0);rt_key('b',0);rt_key(8,0);rt_key('c',0);rt_key(13,0);agi_vm_resume(&rt_vm);r=agi_vm_run(&rt_vm,100);
    check(!r&&!strcmp(rt.strings[1],"ac")&&rt_vm.vars[202]==99,"name editing, backspace and Enter resume original script");
    r=script(4);check(r==VM_WAIT&&rt.ui==RT_UI_MENU&&rt.item_count==3&&!rt.items[1].enabled,"script defines menu and disabled item");
    rt_key(0,0x50);check(rt.item_selected==2,"menu navigation skips disabled item");
    rt.key=27;rt_key(13,0);check(!rt.ui&&!rt.key&&(rt.controllers[12>>3]&(1u<<(12&7))),"menu selection consumes key and dispatches current-scan controller");
    agi_vm_resume(&rt_vm);r=agi_vm_run(&rt_vm,100);
    check(!r&&rt_vm.vars[203]==98,"script receives menu selection immediately after modal resumes");
    r=command(0x63,1,99,0,0,0);check(!r&&rt.sound_ticks==8&&!agi_vm_flag(&rt_vm,99),"sound durations initialize completion timer");
    rt_vm.vars[10]=255;rt.elapsed_ms=0;
    for(i=0;i<6;++i)rt_step(1);
    check(rt.sound_active&&!agi_vm_flag(&rt_vm,99),"50Hz frames preserve 60Hz sound fractional time");
    rt_step(1);check(!rt.sound_active&&agi_vm_flag(&rt_vm,99),"sound completion flag set at duration boundary");
    command(0x63,1,98,0,0,0);command(0x64,0,0,0,0,0);
    check(agi_vm_flag(&rt_vm,98)&&!rt.sound_active,"stop.sound signals completion");
    check(command(0x63,2,98,0,0,0)==VM_HEADER,"malformed sound offsets rejected");
    rt_vm.vars[11]=59;rt_vm.vars[12]=59;rt_vm.vars[13]=23;rt.second_frames=0;rt_step(50);
    check(!rt_vm.vars[11]&&!rt_vm.vars[12]&&!rt_vm.vars[13]&&rt_vm.vars[14]==1,"clock carries seconds minutes hours and days");
    agi_vm_start(&rt_vm,5);r=rt_step(0);
    check(r==VM_RUNNING&&rt_vm.vars[0]==7&&rt_vm.vars[1]==0&&agi_vm_flag(&rt_vm,5)&&rt.horizon==36,
          "room event resets scene state and restarts logic 0 with first-scan flag");
    agi_vm_stop(&rt_vm);resource_cache_status(&info);check(!info.pinned,"runtime fixture operations release all resources");
    (void)value;(void)saved2;(void)saved3;
}
static void game(void) {
    uint16_t i,start,elapsed,peak=0,total=0;uint8_t r;
    strcpy(directory,"games/sq1");r=rt_init(directory);check(!r,"original SQ1 runtime initializes");if(r)return;
    for(i=0;i<sizeof(expected_sounds)/sizeof(expected_sounds[0]);++i){
        r=command(0x63,expected_sounds[i].number,200,0,0,0);
        if(r||rt.sound_ticks!=expected_sounds[i].ticks){++failures;sprintf(report,"FAIL sound %u duration\n",(unsigned)expected_sounds[i].number);log_line(report);}
    }
    check(i==50,"50 original sound resources matched host duration expectations");
    command(0x64,0,0,0,0,0);
    start=next_clock();r=scan();elapsed=next_clock()-start;
    sprintf(report,"TIMING startup=%u frames\n",elapsed);log_line(report);
    sprintf(report,"SQ1 first scan r=%u room=%u logic=%u pc=%u op=%02x\n",(unsigned)r,(unsigned)rt_vm.vars[0],(unsigned)rt_vm.logic,rt_vm.pc,(unsigned)rt_vm.opcode);log_line(report);
    check(!r&&rt_vm.vars[0]==67&&rt.picture_visible&&rt.menu_count==5&&!strcmp(rt.game_id,"SQ"),"unmodified SQ1 startup enters title room 67 and displays original picture");
    if(r)return;
    dump("build/runtime-title.raw");
    for(i=0;i<24&&!r;++i){start=next_clock();r=scan();elapsed=next_clock()-start;total+=elapsed;if(elapsed>peak)peak=elapsed;}
    sprintf(report,"TIMING 24 title scans total=%u peak=%u frames\n",total,peak);log_line(report);
    rt_actor_read(2,&a);
    check(!r&&(a.flags&ACT_DRAWN),"original title script draws and cycles animated logo");
    dump("build/runtime-title-animated.raw");
    rt_key(27,0);r=scan();
    check(r==VM_WAIT&&rt.ui==RT_UI_MENU&&rt_vm.logic==0,"original SQ1 Escape binding opens script-defined menus");
    rt_key(0,0x4d);rt_key(27,0);r=scan();
    check(!r&&!rt.ui&&rt.menu_selected==1&&rt_vm.vars[0]==67,"closing original File menu consumes Escape without skipping title");
    for(i=0;i<120&&!r;++i){
        rt_actor_read(1,&a);if((a.flags&ACT_DRAWN)&&a.motion)break;
        r=scan();
    }
    intrinsic_di();next_key_sample(0);next_key_sample(7);next_key_sample(0);intrinsic_ei();
    rt_actor_render();
    if(next_key()==7)rt_key(27,0);
    r=scan();
    check(i<120&&r==VM_WAIT&&rt.ui==RT_UI_MENU,"queued Shift+1 opens menu during original moving title glow");
    rt_key(27,0);r=scan();
    rt_key(' ',0);
    for(i=0;i<10 && !rt.ui && !r;++i)r=scan();
    sprintf(report,"SQ1 input r=%u room=%u ui=%u logic=%u pc=%u op=%02x\n",(unsigned)r,(unsigned)rt_vm.vars[0],(unsigned)rt.ui,(unsigned)rt_vm.logic,rt_vm.pc,(unsigned)rt_vm.opcode);log_line(report);
    check(r==VM_WAIT&&rt_vm.vars[0]==69&&rt.ui==RT_UI_STRING,"Space skips title through original scripts to player-name prompt");
}
int main(void) {
    uint8_t i,guard=1;uint16_t start;uint32_t spins=0;
    memset((void *)0xfb00,0xa5,16);next_init();next_text(0,30,"RUNTIME TESTS RUNNING");
    errno=0;log_file=esx_f_open("build/runtime-tests.txt",ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);if(errno)for(;;){}
    next_clock_start();authored();
    i=runtime_feedback_tests();
    check(i&1,"menu close/selection restores complete 80 KB framebuffer");
    check(i&2,"menu restoration handles picture origin at lines 0 and 4");
    check(i&4,"text-mode menu restores background and persistent text");
    check(i&32,"menu navigation preserves uncovered pixels and bank mappings");
    check(i&8,"brief key edges survive redraw, held key queues once");
    check(i&16,"key queue overflow bounded and circular buffer reusable");
    check(i&64,"graphics transitions, dialogue wrapping and pixel restoration");
    check(i&128,"full SQ1 intro loop clears story text before credits");
    game();
    check(parser_tests(),"parser and opening-room commands");
    next_clock_start();start=next_clock();while(next_clock()==start && ++spins<100000UL){} intrinsic_di();
    check(spins<100000UL,"resident IM2 timer runs while test code is banked");
    for(i=0;i<16;++i)if(((uint8_t *)0xfb00)[i]!=0xa5)guard=0;
    check(guard,"stack boundary intact across nested overlays, modal UI and interrupts");
    sprintf(report,"SUMMARY checks=%u failures=%u\n",checks,failures);log_line(report);esx_f_close(log_file);
    next_text(0,28,failures?"RUNTIME TESTS FAILED":"RUNTIME TESTS PASSED");
    sprintf(report,"CHECKS %u  FAILURES %u",checks,failures);next_text(0,29,report);for(;;){}
}
