#ifndef NEXT_SOUND_H
#define NEXT_SOUND_H
#include <stdint.h>
#define SOUND_PAGE 91
#define SOUND_BASE 0x6600u
#define SOUND_CAPACITY 6656u
typedef struct { uint16_t at,left; } sound_channel;
/* Foreground may inspect these only with interrupts disabled. */
extern sound_channel sound_channels[4];
extern volatile uint8_t sound_playing,sound_done;
extern uint8_t sound_registers[2][11];
extern volatile uint16_t sound_frames,sound_ticks,sound_writes;
extern volatile uint8_t sound_peak_events;
/* Harness controls: manual frames, or clock-only baseline. Both default off. */
extern uint8_t next_sound_manual,next_sound_bypass;
void next_sound_frame(void); /* resident; IRQ or foreground with IRQs disabled */
void next_sound_begin(const uint16_t *offsets);
void next_sound_halt(void);  /* preserves interrupt enable state */
uint8_t rt_sound_start(uint8_t id,uint8_t flag);
void rt_sound_stop(void);
void rt_sound_poll(void);
#endif
