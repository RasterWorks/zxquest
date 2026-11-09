/* Foreground host controls. Never reset or reinitialize from inside the VM. */
#include <string.h>
#include <stddef.h>
#include <arch/zxn.h>
#include <intrinsic.h>
#include <z80.h>
#include "runtime.h"
#include "sound.h"
#include "resource_cache_internal.h"
uint8_t rt_control_action,rt_paused;
static uint8_t control,selection,volumes[4];
extern uint16_t rt_dialog(uint8_t draw);
extern void sound_ay(uint16_t pair) __z88dk_fastcall;
static void pause_audio(uint8_t pause) {
    uint8_t i,reg,iff=z80_get_int_state();intrinsic_di();
    for(i=0;i<4;++i){reg=i==3?19:8+i;
        if(pause)volumes[i]=sound_registers[i==3][i==3?8:8+i];
        sound_ay(((uint16_t)reg<<8)|(pause?0:volumes[i]));
    }
    rt_paused=pause;z80_set_int_state(iff);
}
static void panel(void) {
    if(control==0x80)strcpy(rt_buffer,"Restart this game?\n\nENTER: restart\nBREAK: keep playing");
    else if(control==0x86)strcpy(rt_buffer,"Quit this game?\nUnsaved progress will be lost.\nThe Next will reboot.\n\nENTER: quit   BREAK: keep playing");
    else if(control==0x88)strcpy(rt_buffer,"Game paused.\n\nPress a key to continue.");
    else {
        strcpy(rt_buffer,"JOYSTICK\n\n  Keyboard only\n  Joystick port 1\n  Joystick port 2\n\nArrows: choose   ENTER: apply\nBREAK: cancel\n\nRelease stick to stop.\nFire: ENTER   Fire 2: BREAK");
        /* Mark selected option without changing geometry. */
        {char *p=strstr(rt_buffer,"  Keyboard");uint8_t i;for(i=0;i<selection;++i)p=strchr(p,'\n')+1;p[0]='>';}
    }
    rt_dialog(1);
}
static void close_panel(void) {
    rt_modal(RT_MODAL_RESTORE);rt.ui=rt.key=rt.key_pending=rt_vm.vars[19]=0;
}
uint8_t rt_control(uint8_t op,uint8_t key,uint8_t scan) {
    if(op){
        control=op;
        if(op==0x86 && key){rt_control_action=1;return VM_WAIT;}
        if(op==0x80 && agi_vm_flag(&rt_vm,16)){rt_control_action=2;return VM_WAIT;}
        selection=rt_joystick_mode;rt_modal(RT_MODAL_CAPTURE);
        rt.ui=RT_UI_CONTROL;
        if(op==0x88)pause_audio(1);
        panel();return VM_WAIT;
    }
    if(control==0x88){if(key || scan){pause_audio(0);close_panel();}return VM_DONE;}
    if(key==27){close_panel();return VM_DONE;}
    if(control==0x8b && (scan==0x48 || scan==0x50)){
        if(scan==0x48)selection=selection?selection-1:2;else selection=(selection+1)%3;
        panel();return VM_DONE;
    }
    if(key!=13)return VM_DONE;
    close_panel();
    if(control==0x8b)rt_joystick_config(selection);
    else rt_control_action=control==0x86?1:2;
    return VM_DONE;
}
uint8_t rt_control_apply(void) {
    char path[101];uint8_t r,sound_on,saved,i;
    /* Restart retains interpreter menu/key definitions. SQ1 skips set.menu
       when restart flag 6 is set. Page 91 scratch survives rt_init. */
    uint16_t bindings=offsetof(rt_state,ui)-offsetof(rt_state,keys);
    if(rt_control_action==1){rt_sound_stop();agi_vm_stop(&rt_vm);return VM_WAIT;}
    strcpy(path,cache_game_directory);sound_on=agi_vm_flag(&rt_vm,9);
    saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,91);
    memcpy((void *)0x7000,rt.keys,bindings);ZXN_WRITE_REG(0x53,saved);
    r=rt_init(path);if(r)return r;
    ZXN_WRITE_REG(0x53,91);memcpy(rt.keys,(void *)0x7000,bindings);ZXN_WRITE_REG(0x53,saved);
    for(i=0;i<rt.item_count;++i)rt.items[i].enabled=1;
    agi_vm_set_flag(&rt_vm,9,sound_on);agi_vm_set_flag(&rt_vm,6,1);
    return VM_DONE;
}
