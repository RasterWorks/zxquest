#include <im2.h>
#include <intrinsic.h>
#include <stdint.h>
#include <string.h>
#include <input.h>
#include <arch/zxn.h>
#include <z80.h>
#include "sound.h"
volatile uint16_t next_clock_ticks;
static volatile uint8_t keys[32];
volatile uint8_t next_key_head,next_key_tail;
uint8_t next_key_previous;
/* Single producer: IRQ only (or deterministic tests with IRQs disabled).
   The decoder gate temporarily maps code bank 52 and restores it; no VM
   or filesystem access is permitted from this path. */
void next_key_sample(uint8_t key) {
    uint8_t next;
    if(key && key!=next_key_previous) {
        next=(next_key_head+1)&31;
        if(next!=next_key_tail) { keys[next_key_head]=key; next_key_head=next; }
    }
    next_key_previous=key;
}
extern uint8_t next_key_decode(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
IM2_DEFINE_ISR(runtime_interrupt) {
    uint8_t selector,arrows,extra,extend,key,left,right;
    ++next_clock_ticks;selector=z80_inp(0x243b);
    arrows=ZXN_READ_REG(0xb0);extra=ZXN_READ_REG(0xb1);
    key=in_inkey();
    extend=!(z80_inp(0xfefe)&1) && !(z80_inp(0x7ffe)&2);
    left=z80_inp(0xf7fe);right=z80_inp(0xeffe);
    key=next_key_decode(key,arrows,extra,left,right,extend);
    z80_outp(0x243b,selector);next_key_sample(key);
    if(!next_sound_manual && !next_sound_bypass)next_sound_frame();
}
uint8_t next_key(void) {
    uint8_t key=0;
    intrinsic_di();
    if(next_key_tail!=next_key_head) { key=keys[next_key_tail]; next_key_tail=(next_key_tail+1)&31; }
    intrinsic_ei(); return key;
}
uint16_t next_clock(void) {
    uint16_t n; intrinsic_di(); n=next_clock_ticks; intrinsic_ei(); return n;
}
