#include <arch/zxn.h>
#include "runtime.h"
uint8_t rt_joystick_mode,rt_joystick_owned;
static uint8_t previous,saved_mode,previous_direction,previous_ui;
static uint16_t repeat_at;
void rt_joystick_config(uint8_t mode) {
    uint8_t value=ZXN_READ_REG(5);
    if(rt_joystick_mode)value=(value&5)|saved_mode;
    saved_mode=value&0xfa;
    if(mode==1)value=(value&0x37)|0x40;
    if(mode==2)value=(value&0xcd)|0x02; /* port 2 -> Kempston 2 */
    ZXN_WRITE_REG(5,value);rt_joystick_mode=mode;
    if(rt_joystick_owned && rt.player_control)rt_vm.vars[6]=0;
    rt_joystick_owned=previous_direction=previous_ui=0;previous=0x30;
}
void rt_joystick(uint8_t raw) {
    uint8_t direction=0,scan=0,buttons,changed,was_ui=rt.ui;uint16_t now;
    if(!rt_joystick_mode)return;
    if(raw==255)raw=0; /* absent interface */
    buttons=(raw&~previous)&0x30;previous=raw;
    /* Opposing contacts cancel on their axis. */
    switch(raw&15){
    case 8:case 11:direction=1;break;case 9:direction=2;break;
    case 1:case 13:direction=3;break;case 5:direction=4;break;
    case 4:case 7:direction=5;break;case 6:direction=6;break;
    case 2:case 14:direction=7;break;case 10:direction=8;break;
    }
    changed=direction!=previous_direction || rt.ui!=previous_ui;
    previous_direction=direction;previous_ui=rt.ui;now=next_clock();
    if(rt.ui || !rt.gfx){
        rt_joystick_owned=0;
        if(direction && (changed || (uint16_t)(now-repeat_at)>=10)){
            scan=direction==1||direction==2||direction==8?0x48:direction>=4&&direction<=6?0x50:direction==3?0x4d:0x4b;
            rt_key(0,scan);repeat_at=now;
        }
    }else if(rt.player_control){
        if(direction){rt_vm.vars[6]=direction;rt_joystick_owned=1;}
        else if(rt_joystick_owned){rt_vm.vars[6]=0;rt_joystick_owned=0;}
    }else rt_joystick_owned=0;
    if(was_ui!=rt.ui)return; /* One press must not dismiss and also act in the game. */
    if(buttons&0x20)rt_key(27,0);else if(buttons&0x10)rt_key(13,0);
}
