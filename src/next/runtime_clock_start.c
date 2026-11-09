#include <im2.h>
#include <intrinsic.h>
#include <stdint.h>
#include <string.h>
#include <z80.h>
extern volatile uint16_t next_clock_ticks;
extern volatile uint8_t next_key_head,next_key_tail;
extern uint8_t next_key_previous;
extern void runtime_interrupt(void);
void next_clock_start(void) {
    intrinsic_di(); next_clock_ticks=0; next_key_head=next_key_tail=next_key_previous=0;
    memset((void *)0xf800,0xf9,257);
    *((uint8_t *)0xf9f9)=0xc3; *((void **)(0xf9fa))=runtime_interrupt;
    im2_init((void *)0xf800); intrinsic_ei();
}

/* IRQ decoder: all state is resident; code is entered through a scoped MMU4
   gate. No resources or mutable foreground scratch are used here. */
static uint8_t held_function,armed,arrow_old,arrow_code,suppress;
void next_key_reset(void) {
    uint8_t iff=z80_get_int_state();intrinsic_di();
    held_function=armed=arrow_old=arrow_code=0;suppress=1;
    next_key_head=next_key_tail=next_key_previous=0;z80_set_int_state(iff);
}
uint8_t next_key_decode(uint8_t key,uint8_t arrows,uint8_t extra,uint8_t left,uint8_t right,uint8_t extend) {
    static const uint8_t direction[16]={0,3,7,0,5,4,6,5,1,2,8,1,0,3,7,0};
    uint8_t i,mask,dir;
    left=(~left)&31;right=(~right)&31;
    if(extra==255)extra=0;if(arrows==255)arrows=0;
    if(suppress){if(!(key|arrows|extra|left|right|extend))suppress=0;return 0;}
    /* Break/cancel must win even while a function digit is still held. */
    if((extra&32) || key==7 || key==27){held_function=armed=0;return 7;}
    if(extra&16){held_function=armed=0;return 138;} /* INV VIDEO */
    if(extra&128){held_function=armed=0;return 12;}
    if(held_function){
        if(held_function>=138 && key)return held_function;
        if(left || right)return held_function;
        held_function=0;
    }
    /* Edit is held, never a delayed tap. Retain Extend/CSpect Tab as a
       compatibility alias; its synthetic pulse uses a one-shot prefix. */
    if(extra&64)armed=2;else if(armed==2)armed=0;
    if((extra&1) || extend)armed=1;
    if((key>=8 && key<=12) || (arrows&15))armed=0;
    if(armed){
        if(key=='q'||key=='Q'||key=='j'||key=='J'){
            held_function=(key=='q'||key=='Q')?139:140;armed=0;return held_function;
        }
        for(i=0;i<5;++i){
            if(left&(1<<i)){armed=0;held_function=128+i;return held_function;}
            if(right&(1<<i)){armed=0;held_function=137-i;return held_function;}
        }
        if((extra&65)||extend)return 0;
        if(key|arrows|extra)armed=0;
    }
    mask=arrows&15;
    if(mask){
        if(mask!=arrow_old){
            /* A new press chooses a direction; pure staggered releases retain
               the chosen diagonal until all arrows are up. */
            if(!arrow_old || (mask&~arrow_old)){
                dir=direction[mask];arrow_code=(arrow_old||!dir?160:144)+dir;
            }
            arrow_old=mask;
        }
        return arrow_code;
    }
    arrow_old=arrow_code=0;
    if(extra&4)return 0; /* Graph is not an Inventory shortcut. */
    if(arrows&16)return '.';if(arrows&32)return ',';
    if(arrows&64)return '"';if(arrows&128)return ';';
    return key;
}
