/* Resident AY player. No allocation, filesystem, resource cache, code banks,
   VM calls, division or conversion in the interrupt. Prepared streams live in
   page 91; the MMU3 mapping AND NextReg selector are restored on every tick. */
#include <arch/zxn.h>
#include <z80.h>
#include <intrinsic.h>
#include "runtime.h"
#include "sound.h"
sound_channel sound_channels[4];
volatile uint8_t sound_playing,sound_done;
uint8_t sound_registers[2][11];
volatile uint16_t sound_frames,sound_ticks,sound_writes;
volatile uint8_t sound_peak_events;
uint8_t next_sound_manual,next_sound_bypass;
uint8_t sound_fraction,sound_noise_mode;
uint8_t sound_ay_chip=255;

/* H = shadow-register index (AY1 0..10, AY2 11..21), L = value.
   AY writes are the hot path. This leaf avoids three C port-call adapters
   per register, writes only changes, and clobbers no IX/IY registers. */
void sound_ay(uint16_t pair) __z88dk_fastcall __naked {
    (void)pair;
__asm
    ld e,l
    ld d,h
    ld l,h
    ld h,0
    ld bc,_sound_registers
    add hl,bc
    ld a,e
    cp (hl)
    ret z
    ld (hl),a
    ld a,d
    cp 11
    jr c,ay_first
    sub 11
    ld d,a
    ld a,254
    jr ay_write
ay_first:
    ld a,255
ay_write:
    ld bc,0xfffd
    ld h,a
    ld a,(_sound_ay_chip)
    cp h
    jr z,ay_selected
    ld a,h
    ld (_sound_ay_chip),a
    out (c),a
ay_selected:
    out (c),d
    ld b,0xbf
    out (c),e
    ld hl,(_sound_writes)
    inc hl
    ld (_sound_writes),hl
    ret
__endasm;
}
#define AY(chip,reg,value) sound_ay(((uint16_t)((chip)*11+(reg))<<8)|(uint8_t)(value))
void sound_quiet(void) {
    AY(0,8,0);AY(0,9,0);AY(0,10,0);AY(1,8,0);
}
void next_sound_halt(void) {
    uint8_t iff=z80_get_int_state();intrinsic_di();
    if(sound_playing)sound_done=1;
    sound_playing=0;sound_quiet();z80_set_int_state(iff);
}
static void noise(void) {
    uint16_t period;
    uint8_t rate=sound_noise_mode&3;
    if(rate==3) {
        period=sound_registers[0][4]|((uint16_t)sound_registers[0][5]<<8);
        period=(sound_noise_mode&4)?period>>1:(period*15u)>>1;
    } else if(sound_noise_mode&4)period=rate?31:16;
    else period=rate==0?235:rate==1?469:939;
    /* AY noise's five-bit divider saturates at low rates. Periodic SN noise
       is a square-wave approximation at the 15-stage repetition frequency. */
    if(!period)period=1;
    if(sound_noise_mode&4) {
        if(period>31)period=31;AY(1,6,period);AY(1,7,0x37);
    } else {
        if(period>4095)period=4095;
        AY(1,0,period);AY(1,1,period>>8);AY(1,7,0x3e);
    }
}
/* One tick, at most one event per channel. Zero-duration events are rejected
   during preparation, so no unbounded stream walk is possible here. */
void sound_tick(void) {
    uint8_t ch,live=0,events=0,noise_dirty=0;
    uint8_t *p;uint16_t duration;sound_channel *c=sound_channels;
    for(ch=0;ch<4;++ch,++c) {
        if(!c->at)continue;
        if(c->left)--c->left;
        if(!c->left) {
            ++events;p=(uint8_t *)c->at;
            duration=p[0]|((uint16_t)p[1]<<8);
            if(duration==65535) {
                c->at=0;AY(ch==3, ch==3?8:8+ch,0);continue;
            }
            c->left=duration;c->at+=5;
            if(ch==3) { sound_noise_mode=p[2];noise_dirty=1;AY(1,8,p[4]); }
            else {
                AY(0,ch*2,p[2]);AY(0,ch*2+1,p[3]);AY(0,8+ch,p[4]);
                if(ch==2)noise_dirty=1;
            }
        }
        ++live;
    }
    if(noise_dirty)noise();
    if(events>sound_peak_events)sound_peak_events=events;
    if(!live){sound_playing=0;sound_done=1;}
}
void next_sound_frame(void) {
    if(rt_paused)return;
    uint8_t selector,saved;
    if(!sound_playing)return;
    ++sound_frames;
    /* Only read the atomic resident flag byte; foreground publishes end flags. */
    if(!(rt_vm.flags[1]&2)){sound_playing=0;sound_done=1;sound_quiet();return;}
    selector=z80_inp(0x243b);saved=ZXN_READ_REG(0x53);
    ZXN_WRITE_REG(0x53,SOUND_PAGE);
    sound_tick();++sound_ticks;
    if(++sound_fraction==5){sound_fraction=0;if(sound_playing){sound_tick();++sound_ticks;}}
    ZXN_WRITE_REG(0x53,saved);z80_outp(0x243b,selector);
}
