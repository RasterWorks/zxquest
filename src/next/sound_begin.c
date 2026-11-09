/* Foreground-only AY setup. Playback and its callbacks remain resident. */
#include <arch/zxn.h>
#include <z80.h>
#include <intrinsic.h>
#include "sound.h"
extern uint8_t sound_fraction,sound_noise_mode,sound_ay_chip;
extern void sound_ay(uint16_t pair) __z88dk_fastcall;
extern void sound_quiet(void);
extern void sound_tick(void);
#define AY(chip,reg,value) sound_ay(((uint16_t)((chip)*11+(reg))<<8)|(uint8_t)(value))
void next_sound_begin(const uint16_t *offsets) {
    uint8_t ch,saved,selector,iff=z80_get_int_state();intrinsic_di();
    selector=z80_inp(0x243b);saved=ZXN_READ_REG(0x53);
    /* Enable TurboSound and choose AY volume response, preserving other bits. */
    ZXN_WRITE_REG(0x08,ZXN_READ_REG(0x08)|2);
    ZXN_WRITE_REG(0x06,(ZXN_READ_REG(0x06)&0xfc)|1);
    sound_ay_chip=0;
    for(ch=0;ch<11;++ch)sound_registers[0][ch]=sound_registers[1][ch]=255;
    sound_quiet();AY(1,9,0);AY(1,10,0);AY(0,7,0x38);AY(1,7,0x3f);
    sound_frames=sound_ticks=sound_writes=0;sound_peak_events=0;
    for(ch=0;ch<4;++ch){sound_channels[ch].at=SOUND_BASE+offsets[ch];sound_channels[ch].left=0;}
    sound_fraction=0;sound_noise_mode=4;sound_done=0;sound_playing=1;
    ZXN_WRITE_REG(0x53,SOUND_PAGE);sound_tick(); /* prime now; duration starts now */
    ZXN_WRITE_REG(0x53,saved);z80_outp(0x243b,selector);
    z80_set_int_state(iff);
}
