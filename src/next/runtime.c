/* AGI game-cycle/command adaptation from Sarien.
 * Copyright (C) 1999-2003 Stuart George and Claudio Matsuoka
 * Distributed under the GNU General Public License; see COPYING.
 * ZXQuest replaces host globals/pointers with banked adapters and bounded state.
 */
#include <string.h>
#include <stdio.h>
#include "runtime.h"
#include "platform.h"
#include "sound.h"
#include "save.h"
#include <intrinsic.h>
#include <z80.h>
/* runtime_state.c shares overlay 53; this call needs no resident gate. */
extern void rt_show_picture(void);
extern void next_key_reset(void);
/* Optional emulator profiling: VM, actor update, render, input/status frames.
   Disabled in interactive builds unless explicitly enabled by a harness. */
uint8_t rt_profile_on;
uint16_t rt_profile[4];

static void enter_room(uint8_t n) {
    rt_actor ego;
    rt_sound_stop(); rt_actor_reset(0); rt_actor_read(0,&ego);
    rt.player_control=1; rt.horizon=36;
    rt.block_on=0;
    rt_vm.vars[1]=rt_vm.vars[0]; rt_vm.vars[0]=n;
    rt_vm.vars[4]=rt_vm.vars[5]=0; rt_vm.vars[16]=ego.view;
    switch(rt_vm.vars[2]) {
    case 1: ego.y=167; break; case 2: ego.x=0; break;
    case 3: ego.y=37; break; case 4: ego.x=160-ego.width; break;
    }
    ego.flags=ACT_UPDATE; ego.old_drawn=0; rt_actor_write(0,&ego);
    rt_vm.vars[2]=0; rt_vm.vars[9]=0; agi_vm_reset_scans(&rt_vm);
    agi_vm_set_flag(&rt_vm,5,1); agi_vm_set_flag(&rt_vm,2,0);
    memset(rt.controllers,0,32); rt.key=0;
    rt.picture_valid=rt.picture_visible=0; rt.dirty=0;
}
uint8_t rt_init(const char *directory) {
    uint8_t r;
    next_key_reset();
    rt_sound_stop(); agi_vm_stop(&rt_vm); memset(&rt,0,sizeof(rt));rt_save_identity_valid=0;
    rt_control_action=rt_paused=rt_joystick_owned=0;
    r=resource_cache_init(directory); if(r) return VM_IO;
    agi_vm_init(&rt_vm,&next_logic_io,rt_command_gate,rt_test_gate);
    rt.horizon=36; rt.gfx=1; rt.fg=15; rt.cursor='_'; rt.input_line=22;
    rt.print_line=1; rt.random=0x7341; rt.elapsed_ms=100;
    next_picture_top=40; rt_actor_reset(1); rt_text_clear();
    r=rt_objects(directory); if(r) return r;
    r=rt_parser_init(directory);if(r)return r;
    rt_vm.vars[10]=2; rt_vm.vars[20]=0; rt_vm.vars[22]=1; rt_vm.vars[26]=3; rt_vm.vars[24]=38;
    agi_vm_set_flag(&rt_vm,5,1); agi_vm_set_flag(&rt_vm,11,1); agi_vm_set_flag(&rt_vm,9,1);
    return VM_DONE;
}
void rt_key(uint8_t ascii,uint8_t scan) {
    uint8_t i,controller;
    if(rt.ui) { rt.error=rt_ui_key(ascii,scan); return; }
    rt.key_pending=ascii ? ascii : 1;
    /* A script can poll have.key inside one long-running LOGIC scan. */
    if(rt_vm.status==VM_RUNNING)rt.key=rt.key_pending;
    rt_vm.vars[19]=ascii;
    /* Text screens own their dismissal key, including bound controllers. */
    if(!rt.gfx)return;
    if(rt.player_control) {
        controller=scan==0x48 ? 1 : scan==0x4d ? 3 : scan==0x50 ? 5 : scan==0x4b ? 7 : 0;
        if(controller) {rt_joystick_owned=0;rt_vm.vars[6]=rt_vm.vars[6]==controller ? 0 : controller;}
    }
    for(i=0;i<rt.key_count;++i) {
        if(rt.keys[i].ascii==ascii && rt.keys[i].scan==scan) {
            controller=rt.keys[i].controller; rt.pending[controller>>3]|=1u<<(controller&7);return;
        }
    }
    if(rt.gfx && rt.input_enabled && ascii && ascii!=27){rt.key_pending=0;rt.error=rt_parser_key(ascii);}
}
uint8_t rt_step(uint16_t frames) {
    uint8_t r,i; uint32_t ms,ticks; uint16_t delay,started=0;
    if(rt_control_action)return rt_control_apply();
    if(next_sound_manual) {
        uint16_t f;uint8_t iff=z80_get_int_state();intrinsic_di();
        for(f=0;f<frames;++f)next_sound_frame();z80_set_int_state(iff);
    }
    rt_sound_poll();
    if(rt.error) { next_sound_halt();return rt.error; }
    if(rt.ui) return VM_WAIT;
    ticks=(uint32_t)rt.second_frames+frames;
    rt.second_frames=ticks%50;
    while(ticks>=50) {
        ticks-=50;
        if(++rt_vm.vars[11]==60) { rt_vm.vars[11]=0;
            if(++rt_vm.vars[12]==60) { rt_vm.vars[12]=0;
                if(++rt_vm.vars[13]==24) { rt_vm.vars[13]=0; ++rt_vm.vars[14]; }
            }
        }
    }
    ms=(uint32_t)rt.elapsed_ms+(uint32_t)frames*20; rt.elapsed_ms=ms>65535 ? 65535 : ms;
    if(rt_vm.status==VM_WAIT) agi_vm_resume(&rt_vm);
    if(rt_vm.status!=VM_RUNNING) {
        delay=rt_vm.vars[10] ? (uint16_t)rt_vm.vars[10]*50 : 20;
        if(rt.elapsed_ms<delay) return VM_DONE;
        r=rt_parser_cycle(1);if(r)return rt.error=r;
        rt.elapsed_ms=0; rt.key=rt.key_pending; rt.key_pending=0;
        for(i=0;i<32;++i) { rt.controllers[i]=rt.pending[i]; rt.pending[i]=0; }
        r=agi_vm_start(&rt_vm,0); if(r!=VM_RUNNING) return rt.error=r;
    }
    if(rt_profile_on)started=next_clock();
    r=agi_vm_run(&rt_vm,200);
    if(rt_profile_on)rt_profile[0]+=next_clock()-started;
    if(r==VM_ROOM) { enter_room(rt_vm.room); return agi_vm_start(&rt_vm,0); }
    if(r==VM_RUNNING || r==VM_WAIT) return r;
    if(r) return rt.error=r;
    ++rt.cycles;
    agi_vm_set_flag(&rt_vm,5,0); agi_vm_set_flag(&rt_vm,6,0); agi_vm_set_flag(&rt_vm,12,0);
    rt_vm.vars[4]=rt_vm.vars[5]=0;
    memset(rt.controllers,0,32); rt.key=0;
    if(rt_profile_on)started=next_clock();
    r=rt_actor_tick();
    if(rt_profile_on){rt_profile[1]+=next_clock()-started;started=next_clock();}
    if(!r && rt.dirty) r=rt_actor_render();
    if(rt_profile_on){rt_profile[2]+=next_clock()-started;started=next_clock();}
    if(!r)r=rt_parser_cycle(0);
    if(rt_profile_on)rt_profile[3]+=next_clock()-started;
    if(r) rt.error=r;
    return r;
}
uint8_t rt_command(agi_vm *v,uint8_t op,const uint8_t *p) {
    uint8_t i,a=p[0],b=p[1]; uint16_t span;
    (void)v;
    if(op==0x74 || op==0x75 || op==0x77 || op==0x78 || op==0x89 || op==0x8a)return rt_parser_command(op,p);
    if(op>=0x21 && op<=0x61) return rt_actor_command(op,p);
    if(op>=0x65 && op<=0x76) return rt_text_command(op,p);
    if(op>=0x9c && op<=0xa1) return rt_text_command(op,p);
    if(op==0x7c || op==0x81 || op==0xa2)return rt_inventory(op,a,b);
    if(op==0x7d || op==0x7e)return rt_save_ui(op,0,0);
    if(op==0x80 || op==0x86 || op==0x88 || op==0x8b)return rt_control(op,a,0);
    switch(op) {
    case 0x18: case 0x1e: case 0x1f: case 0x62: return next_logic_command(&rt_vm,op,p);
    case 0x19: return rt_picture(rt_vm.vars[a],0);
    case 0x1c: return rt_picture(rt_vm.vars[a],1);
    case 0x1a:
        rt.picture_visible=1; rt.dirty|=2; /* Fresh composition for show.pic. */
        i=rt_actor_render(); if(!i) rt_show_picture(); return i;
    case 0x1b: case 0x20: case 0x99: return VM_DONE; /* cache is free to evict unpinned data */
    case 0x1d: next_show_picture(1); return VM_DONE;
    case 0x63: return rt_sound_start(a,b);
    case 0x64: rt_sound_stop(); return VM_DONE;
    case 0x79:
        for(i=0;i<rt.key_count;++i) if(rt.keys[i].ascii==a && rt.keys[i].scan==b && rt.keys[i].controller==p[2]) return VM_DONE;
        if(rt.key_count==RT_KEYS) return VM_ARGUMENT;
        rt.keys[rt.key_count].ascii=a; rt.keys[rt.key_count].scan=b;
        rt.keys[rt.key_count++].controller=p[2]; return VM_DONE;
    case 0x82:
        if(a>b) return VM_ARGUMENT;
        rt.random=rt.random*25173u+13849u; span=(uint16_t)b-a+1;
        /* Use high bits for small ranges: LCG low bits alone repeat every
           four draws for random(0,3), which the slot-machine reels use. */
        rt_vm.vars[p[2]]=a+((rt.random>>8)|(rt.random<<8))%span; return VM_DONE;
    case 0x83: rt.player_control=0; return VM_DONE;
    case 0x84: return rt_actor_command(op,p);
    case 0x8e: rt.script_size=a; return VM_DONE;
    case 0x8f:
        i=rt_format(rt_vm.logic,a,rt_buffer,sizeof(rt_buffer)); if(i) return i;
        strncpy(rt.game_id,rt_buffer,8); rt.game_id[8]=0; return VM_DONE;
    case 0x95: rt.trace_enabled=1; return VM_DONE;
    case 0x96: rt.trace_logic=a; rt.trace_top=b; rt.trace_height=p[2]; return VM_DONE;
    case 0x97: case 0x98: return rt_text_command(op,p);
    case 0x7a: case 0x7b: case 0x93: case 0x94: return rt_actor_command(op,p);
    case 0xa3: case 0xa4: return VM_DONE; /* text uses its own overlay, no dialogue resource */
    default: return VM_UNSUPPORTED;
    }
}
uint8_t rt_test(agi_vm *v,uint8_t op,const uint8_t *p,uint16_t at,uint8_t *value) {
    (void)v; (void)at;
    switch(op) {
    case 9: case 10:
        if(p[0]>=rt.objects) return VM_ARGUMENT;
        *value=rt.locations[p[0]]==(op==9 ? 255 : rt_vm.vars[p[1]]); return VM_DONE;
    case 11: case 16: case 17: case 18: return rt_actor_test(op,p,value);
    case 12: *value=(rt.controllers[p[0]>>3]>>(p[0]&7))&1; return VM_DONE;
    case 13: *value=rt.key!=0; rt.key=0; if(*value)rt.key_pending=0; return VM_DONE;
    case 14:
        /* Most scans have no sentence. Avoid parser/data-bank switches for
           every said condition when the standard AGI flags rule it out. */
        if(!(rt_vm.flags[0]&4) || (rt_vm.flags[0]&16) || rt_vm.vars[9]){*value=0;return VM_DONE;}
        return rt_parser_test(op,p,at,value);
    case 15:return rt_parser_test(op,p,at,value);
    default: return VM_UNSUPPORTED;
    }
}

/* Foreground-only frontend helpers share the runtime overlay. */
void rt_host_key(uint8_t key) {
    uint8_t ascii=key,scan=0;
    if((key>=145 && key<=152) || (key>=160 && key<=168)){
        uint8_t d=key>=160?key-160:key-144;
        if(rt.gfx && !rt.ui && rt.player_control){
            rt_joystick_owned=0;rt_vm.vars[6]=(key<160 && rt_vm.vars[6]==d)?0:d;
            rt.key_pending=1;rt_vm.vars[19]=0;if(rt_vm.status==VM_RUNNING)rt.key=1;return;
        }
        /* Menus use only cardinal directions; neutral/opposing keys do nothing. */
        scan=d==1?0x48:d==3?0x4d:d==5?0x50:d==7?0x4b:0;
        if(scan)rt_key(0,scan);return;
    }
    if(key>=128 && key<=137){ascii=0;scan=0x3b+key-128;}
    else if(key==139){ascii=0;scan=0x2c;} /* original Alt-Z */
    else if(key==140)ascii=10; /* original Ctrl-J */
    else if(key==138)ascii=9;
    else if(key==7 || key==27)ascii=27;
    else if(key==12)ascii=8;
    else if(key==8){ascii=0;scan=0x4b;}
    else if(key==9){ascii=0;scan=0x4d;}
    else if(key==11){ascii=0;scan=0x48;}
    else if(key==10){ascii=0;scan=0x50;}
    rt_key(ascii,scan);
}
void rt_stop_message(uint8_t result) {
    rt_sound_stop();
    sprintf(rt_buffer,"STOP %u  LOGIC %u  OFFSET %u       ",(unsigned)result,(unsigned)rt_vm.logic,rt_vm.pc);next_text(0,28,rt_buffer);
    sprintf(rt_buffer,"OPCODE %02X  TEST %u                ",(unsigned)rt_vm.opcode,(unsigned)rt_vm.is_test);next_text(0,29,rt_buffer);
    strcpy(rt_buffer,"DEVELOPMENT BUILD - COMMAND PENDING");next_text(0,30,rt_buffer);
}
