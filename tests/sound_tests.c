#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <intrinsic.h>
#include <z80.h>
#include <string.h>
#include <stdio.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "next/sound.h"
static uint8_t file,params[7],copy[64];
static uint16_t checks,failures,offsets[4];
static char directory[40];
static rt_actor motion_reference;
static uint16_t motion_crc,motion_frames;
/* z80_get_int_state returns an opaque NMOS-safe flags byte, not a boolean.
   Inspect the CPU's IFF2 directly for this disabled-interrupt assertion. */
static uint8_t enabled(void) __naked {
__asm
    ld a,i
    ld l,0
    ret po
    inc l
    ret
__endasm;
}
static void line(const char *s) { esx_f_write(file,(void *)s,strlen(s)); }
static void check(uint8_t ok,const char *name) {
    ++checks;if(!ok)++failures;sprintf(rt_buffer,"%s %s\n",ok?"PASS":"FAIL",name);line(rt_buffer);
}
static void frame(void) { intrinsic_di();next_sound_frame();intrinsic_ei();rt_sound_poll(); }
static uint16_t pitch(uint8_t ch) { return sound_registers[0][ch*2]|((uint16_t)sound_registers[0][ch*2+1]<<8); }
static uint8_t quiet(void) { return !(sound_registers[0][8]|sound_registers[0][9]|sound_registers[0][10]|sound_registers[1][8]); }
static uint8_t hardware(uint8_t chip,uint8_t reg) {
    uint8_t v;intrinsic_di();z80_outp(0xfffd,255-chip);z80_outp(0xfffd,reg);v=z80_inp(0xfffd);intrinsic_ei();return v;
}
static void wait_frames(uint16_t n) { uint16_t start=next_clock();while((uint16_t)(next_clock()-start)<n){} }
static uint16_t raster(void) {
    uint8_t high,low;
    do { high=ZXN_READ_REG(0x1e)&1;low=ZXN_READ_REG(0x1f); }while(high!=(ZXN_READ_REG(0x1e)&1));
    return ((uint16_t)high<<8)|low;
}
static uint16_t checksum(uint8_t first,uint8_t count) {
    uint8_t saved=ZXN_READ_REG(0x53),p;uint16_t i,crc=0xffff;
    for(p=first;p<first+count;++p){ZXN_WRITE_REG(0x53,p);for(i=0;i<8192;++i)crc=(crc<<1)|(crc>>15),crc^=*((uint8_t *)(0x6000u+i));}
    ZXN_WRITE_REG(0x53,saved);return crc;
}
static void authored(void) {
    uint8_t i,s2,s3,s4,ok;uint16_t writes;
    strcpy(directory,"build/sound-fixtures");check(!rt_init(directory),"sound fixture initializes");
    check(!rt_sound_start(1,200)&&sound_playing&&rt.sound_ticks==6&&!agi_vm_flag(&rt_vm,200),"first events primed immediately; completion flag reset");
    check(pitch(0)==98&&pitch(1)==293&&pitch(2)==391,"three independent converted tone dividers");
    check(sound_registers[0][8]==15&&!sound_registers[0][9]&&sound_registers[0][10]==11,"attenuation and silent notes map to AY volumes");
    check(sound_registers[1][7]==0x37&&sound_registers[1][6]==16&&sound_registers[1][8]==13,"noise uses AY2 independently of tone channels");
    check(hardware(0,0)==98&&hardware(0,8)==15&&hardware(1,6)==16&&hardware(1,8)==13,"CSpect reads back distinct AY1/AY2 hardware registers");
    writes=sound_writes;frame();check(sound_ticks==1&&sound_writes==writes&&sound_playing,"held notes advance without redundant AY writes");
    frame();check(sound_ticks==2&&!sound_registers[0][8]&&sound_registers[1][7]==0x3e,"zero-frequency rest and periodic-noise approximation");
    frame();check(pitch(0)==196&&sound_registers[0][8]==13,"rest ends at exact note boundary");
    frame();check(sound_registers[1][7]==0x37&&sound_registers[1][6]==31,"tone-coupled noise divider is bounded");
    check(sound_playing&&!agi_vm_flag(&rt_vm,200),"four frames have advanced four sound ticks");
    frame();check(sound_ticks==6&&!sound_playing&&agi_vm_flag(&rt_vm,200)&&quiet(),"fifth 50Hz frame advances two ticks; all endings silence");
    check(!rt_sound_start(2,201)&&agi_vm_flag(&rt_vm,201)&&!sound_playing,"four empty channels complete immediately");
    for(i=3;i<=7;++i)check(rt_sound_start(i,202)==VM_HEADER&&!sound_playing&&quiet(),"malformed, truncated, zero, overlapping or oversized sound rejected");
    rt_sound_start(1,203);rt_sound_start(8,204);
    check(agi_vm_flag(&rt_vm,203)&&!agi_vm_flag(&rt_vm,204),"replacement completes previous flag and starts new sound");
    rt_sound_stop();check(agi_vm_flag(&rt_vm,204)&&quiet()&&!sound_playing,"stop.sound silences and completes flag");
    agi_vm_set_flag(&rt_vm,9,0);check(!rt_sound_start(250,205)&&agi_vm_flag(&rt_vm,205)&&!sound_playing,"disabled sound completes without resource IO");
    agi_vm_set_flag(&rt_vm,9,1);rt_sound_start(1,206);agi_vm_set_flag(&rt_vm,9,0);frame();
    check(agi_vm_flag(&rt_vm,206)&&quiet()&&!sound_playing,"clearing flag 9 stops active playback at next interrupt");
    agi_vm_set_flag(&rt_vm,9,1);rt_sound_start(9,207);
    s2=ZXN_READ_REG(0x52);s3=ZXN_READ_REG(0x53);s4=ZXN_READ_REG(0x54);ok=1;
    for(i=0;i<100;++i){
        z80_outp(0x243b,0x52);next_sound_frame();if(z80_inp(0x253b)!=s2)ok=0;
        z80_outp(0x243b,0x53);next_sound_frame();z80_outp(0x253b,61);
        if(ZXN_READ_REG(0x53)!=61)ok=0;ZXN_WRITE_REG(0x53,s3);
    }
    check(ok&&ZXN_READ_REG(0x52)==s2&&ZXN_READ_REG(0x53)==s3&&ZXN_READ_REG(0x54)==s4,"interrupt between NextReg select/access preserves selector and all windows");
    intrinsic_di();next_sound_halt();check(!enabled(),"halt preserves disabled interrupts");intrinsic_ei();
    check(enabled(),"interrupts remain enabled after normal operations");
    rt_sound_start(9,208);check(!rt_init(directory)&&!sound_playing&&quiet(),"runtime reset silences hardware before clearing state");
}
static void realtime(void) {
    uint16_t before,after,scene,actors,i,n;uint8_t ok=1,saved,r;resource_handle h;
    strcpy(directory,"build/runtime-fixtures");check(!rt_init(directory),"graphics stress fixture initializes");
    params[0]=0;rt_vm.vars[0]=0;check(!rt_picture(0,0),"picture decoded before stress baseline");
    scene=checksum(32,8);actors=checksum(60,2);
    strcpy(directory,"build/sound-fixtures");resource_cache_init(directory);rt_sound_start(10,209);
    next_sound_manual=0;wait_frames(25);intrinsic_di();before=sound_ticks;intrinsic_ei();
    wait_frames(25);intrinsic_di();after=sound_ticks;intrinsic_ei();
    check(after-before>=29&&after-before<=31,"real resident IRQ maintains 60Hz while foreground waits");
    rt_sound_stop();rt_sound_start(9,210);
    before=checksum(40,10);rt_modal(RT_MODAL_CAPTURE);rt_modal(RT_MODAL_CLEAR);rt_modal(RT_MODAL_RESTORE);
    check(checksum(40,10)==before&&sound_playing,"active sound preserves full modal capture/restore pixels");
    if(resource_acquire(RESOURCE_SOUND,10,&h))ok=0;
    else {
        for(i=0;i<1000;++i){
            if(resource_copy(h,0,copy,64)||copy[0]!=8||copy[1]!=0)ok=0;
        }
        resource_release(h);
    }
    check(ok&&checksum(32,8)==scene&&checksum(60,2)==actors,"IRQ interleaves cache copies without corrupting pictures, actors or UI state");
    /* Switch game/cache while audio keeps its independent private copy. */
    strcpy(directory,"games/sq1");resource_cache_init(directory);before=sound_ticks;
    check(!rt_picture(1,0),"original picture decodes while sound is active");
    check(sound_playing&&sound_ticks>before,"sound survives resource cache reset and foreground picture decoding");
    strcpy(directory,"build/runtime-fixtures");resource_cache_init(directory);
    before=checksum(40,10);after=sound_ticks;r=agi_vm_start(&rt_vm,4);
    while(r==VM_RUNNING)r=agi_vm_run(&rt_vm,100);
    rt_key(27,0);
    check(r==VM_WAIT&&!rt.ui&&sound_playing&&sound_ticks>after&&checksum(40,10)==before,"original menu capture/navigation/close stays intact with real audio interrupts");
    agi_vm_stop(&rt_vm);
    rt.ui=RT_UI_MESSAGE;before=sound_ticks;wait_frames(10);rt_step(0);
    check(sound_ticks>before&&rt.ui==RT_UI_MESSAGE,"modal dialogues do not pause playback");rt.ui=0;
    rt_sound_stop();next_sound_manual=1;
    saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,91);n=0;
    for(i=0x6600;i<0x8000;++i)n+=*((uint8_t *)i);ZXN_WRITE_REG(0x53,saved);
    sprintf(rt_buffer,"PRIVATE audio checksum=%u\n",n);line(rt_buffer);
}
/* Timing includes loop/measurement/start overhead: an upper bound, without
   subtracting the muted player. IM2 still supplies the clock and keyboard. */
static void benchmark(void) {
    uint16_t start,base,active,i,j,at,lines,maxlines=0;uint8_t saved;
    strcpy(directory,"build/sound-fixtures");resource_cache_init(directory);rt_sound_start(10,211);
    saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,91);
    for(i=0;i<4;++i)offsets[i]=*((uint16_t *)(SOUND_BASE+i*2));ZXN_WRITE_REG(0x53,saved);
    /* All 300 events/voice last one tick: frequent four-channel changes. */
    start=next_clock();for(j=0;j<20;++j){next_sound_begin(offsets);for(i=0;i<200;++i){}}
    base=next_clock()-start;
    start=next_clock();for(j=0;j<20;++j){next_sound_begin(offsets);for(i=0;i<200;++i)next_sound_frame();}
    active=next_clock()-start;
    next_sound_begin(offsets);
    for(i=0;i<200;++i){at=raster();next_sound_frame();lines=raster();lines=lines>=at?lines-at:312+lines-at;if(lines>maxlines)maxlines=lines;}
    sprintf(rt_buffer,"BENCH 4000 frames clock-only=%u player=%u additional=%u; max IRQ-body raster lines=%u (312/frame)\n",base,active,active-base,maxlines);line(rt_buffer);
    /* The average acceptance gate is applied to all 50 original resources in
       sound_game_tests. This pathological event density measures a separate
       worst case; it must still leave the IRQ well inside one frame. */
    check(maxlines<32,"pathological double-tick IRQ body stays below 10 percent of a frame");
    check(sound_peak_events==4,"interrupt work bounded to four events per sound tick");
    rt_sound_stop();
}
static void motion_compare(uint8_t audible) {
    uint16_t start,i,attempts,elapsed,crc;uint8_t r;rt_actor actor;resource_handle h;
    next_sound_manual=1;strcpy(directory,"build/runtime-fixtures");r=rt_init(directory);
    do {r=rt_step(5);}while(r==VM_RUNNING);
    rt_sound_stop();strcpy(directory,"build/sound-fixtures");resource_cache_init(directory);rt_sound_start(9,212);
    strcpy(directory,"build/runtime-fixtures");resource_cache_init(directory);
    if(!resource_acquire(RESOURCE_LOGIC,0,&h))resource_release(h);
    if(!resource_acquire(RESOURCE_VIEW,1,&h))resource_release(h);
    rt.player_control=1;rt_vm.vars[10]=0;rt_key(0,0x4b);
    next_sound_bypass=!audible;next_sound_manual=0;start=next_clock();
    for(i=0;i<16&&!r;++i) {
        if(i==4)rt_key('a',0);if(i==8)rt_key('b',0);
        attempts=0;do {r=rt_step(attempts?0:5);++attempts;}while(r==VM_RUNNING&&attempts<100);
    }
    elapsed=next_clock()-start;rt_actor_read(0,&actor);crc=checksum(40,10);
    next_sound_manual=1;next_sound_bypass=0;rt_sound_stop();
    check(!r&&i==16&&rt_vm.vars[100]==2,"fixed movement and two input events complete with selected IRQ mode");
    if(!audible){motion_reference=actor;motion_crc=crc;motion_frames=elapsed;}
    else {
        check(!memcmp(&motion_reference,&actor,sizeof(actor))&&motion_crc==crc,"clock-only and sound IRQ runs have identical actor state and framebuffer");
        sprintf(rt_buffer,"GAMEPLAY 16 scans clock-only=%u sound=%u frames; input events=2, state/CRC identical\n",motion_frames,elapsed);line(rt_buffer);
    }
}
uint8_t bank_sound_tests(void) {
    uint8_t i,ok=1;strcpy(directory,"build/sound-tests.txt");
    file=esx_f_open(directory,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    memset((void *)0xfb00,0xa5,64);authored();realtime();benchmark();motion_compare(0);motion_compare(1);
    for(i=0;i<64;++i)if(*((uint8_t *)(0xfb00u+i))!=0xa5)ok=0;
    check(ok,"sound, filesystem and nested overlays preserve stack guard");
    sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",checks,failures);line(rt_buffer);esx_f_close(file);return !failures;
}
