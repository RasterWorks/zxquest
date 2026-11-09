/* Actor command/cycle semantics adapted from Sarien.
 * Copyright (C) 1999-2003 Stuart George and Claudio Matsuoka
 * GNU General Public License; see COPYING. Banked actor records are new.
 */
#include <string.h>
#include <arch/zxn.h>
#include "runtime.h"
#include "platform.h"
#include "agi/picture.h"
next_view rt_work_view;
agi_view_cel rt_work_cel;
rt_actor rt_work_actor;
/* These helpers share code page 54 with actor commands. */
extern uint8_t rt_actor_valid(uint8_t,const rt_actor *,int16_t,int16_t,uint8_t);
extern uint8_t rt_actor_fix(uint8_t,rt_actor *);
extern void rt_motion_begin(void);
static uint8_t select(rt_actor *a) {
    agi_view_info info; uint8_t r=next_view_open(&rt_work_view,a->view);
    if(r) return VM_IO;
    r=agi_view_inspect(&rt_work_view.source,&info);
    if(!r) {
        if(a->loop>=info.loops) a->loop=0;
        r=agi_view_select(&rt_work_view.source,a->loop,0,&rt_work_cel);
        if(!r) { if(a->cel>=rt_work_cel.cels) a->cel=0; r=agi_view_select(&rt_work_view.source,a->loop,a->cel,&rt_work_cel); }
    }
    if(!r) { a->width=rt_work_cel.width; a->height=rt_work_cel.height; }
    next_view_close(&rt_work_view); return r ? VM_HEADER : VM_DONE;
}
static void margin_pixel(uint8_t x,uint8_t y,uint8_t priority) {
    uint16_t at=(uint16_t)y*160+x;
    if((agi_pixel_get(at)>>4)>=4)agi_pixel_write(at,0xf0,priority<<4);
}
/* Permanent VIEW stamps become part of the clean scene, including AGI's
   optional control margin. Adapted from Sarien sprite.c add_to_pic. */
static uint8_t add_picture(uint8_t op,const uint8_t *p) {
    uint8_t q[7],i,r,x,y,right,top,band,priority;
    for(i=0;i<7;++i)q[i]=op==0x7b?rt_vm.vars[p[i]]:p[i];
    if(!rt.picture_valid || q[3]>=160 || q[4]>=168 || q[5]>15)return VM_ARGUMENT;
    r=next_view_open(&rt_work_view,q[0]);if(r)return VM_IO;
    r=agi_view_select(&rt_work_view.source,q[1],q[2],&rt_work_cel);
    if(!r)r=agi_view_decode(&rt_work_view.source,&rt_work_cel,0,0);
    if(r){next_view_close(&rt_work_view);return VM_HEADER;}
    x=q[3];y=q[4];if(y+1<rt_work_cel.height)y=rt_work_cel.height-1;if(y>167)y=167;
    top=y+1<rt_work_cel.height?0:y+1-rt_work_cel.height;
    right=(uint16_t)x+rt_work_cel.width>160?159:x+rt_work_cel.width-1;
    priority=q[5]?q[5]:agi_view_priority(y);
    next_scene_restore(0,0,160,168);
    r=agi_view_blit(&rt_work_view.source,&rt_work_cel,x,y,priority);next_view_close(&rt_work_view);
    if(r)return VM_HEADER;
    if(q[6]<4) {
        band=(y/12)*12+1;
        for(i=band;i<=y;++i){margin_pixel(x,i,q[6]);margin_pixel(right,i,q[6]);}
        for(i=x;i<=right;++i){margin_pixel(i,band,q[6]);margin_pixel(i,y,q[6]);}
    }
    next_scene_capture();rt.dirty|=2;r=rt_actor_render();
    if(!r && rt.gfx && rt.picture_visible)next_show_rect(x,top,right-x+1,y-top+1,0);
    return r;
}
void rt_actor_reset(uint8_t initial) {
    uint16_t id;
    for(id=0;id<256;++id) {
        if(initial) memset(&rt_work_actor,0,sizeof(rt_work_actor)); else rt_actor_read(id,&rt_work_actor);
        rt_work_actor.flags=ACT_UPDATE; rt_work_actor.old_drawn=0;
        rt_work_actor.step=rt_work_actor.step_time=rt_work_actor.step_count=rt_work_actor.cycle_time=rt_work_actor.cycle_count=1;
        rt_work_actor.motion=rt_work_actor.direction=rt_work_actor.cycle=0;
        rt_work_actor.ignore_blocks=0;
        rt_actor_write(id,&rt_work_actor);
    }
    memset(rt.occupied,0,32);
}
uint8_t rt_actor_command(uint8_t op,const uint8_t *p) {
    uint8_t id=p[0],b=p[1],r=0; int16_t x,y; uint16_t i; rt_actor other;
    if(op==0x7a || op==0x7b)return add_picture(op,p);
    if(op==0x3f) { rt.horizon=id; return VM_DONE; }
    if(op==0x5a){if(p[0]>p[2] || p[1]>p[3])return VM_ARGUMENT;memcpy(rt.block,p,4);rt.block_on=1;return VM_DONE;}
    if(op==0x5b){rt.block_on=0;return VM_DONE;}
    if(op==0x22) {
        for(i=0;i<256;++i) if(rt.occupied[i>>3]&(1u<<(i&7))) {
            rt_actor_read(i,&rt_work_actor); rt_work_actor.flags&=~(ACT_ANIMATED|ACT_DRAWN); rt_actor_write(i,&rt_work_actor);
        }
        rt.dirty|=1; return VM_DONE;
    }
    if(op>=0x5c && op<=0x61) {
        if(op==0x5d || op==0x60 || op==0x61) id=rt_vm.vars[id];
        if(id>=rt.objects) return VM_ARGUMENT;
        if(op==0x61) rt_vm.vars[b]=rt.locations[id];
        else rt.locations[id]=op==0x5e ? 0 : op<=0x5d ? 255 : rt_vm.vars[b];
        return VM_DONE;
    }
    if(op==0x84)id=0;
    rt_actor_read(id,&rt_work_actor);
    switch(op) {
    case 0x21:
        if(!(rt_work_actor.flags&ACT_ANIMATED)) { rt_work_actor.flags=ACT_ANIMATED|ACT_UPDATE|ACT_CYCLING; rt_work_actor.ignore_blocks=0; rt_work_actor.motion=rt_work_actor.direction=rt_work_actor.cycle=0; }
        break;
    case 0x23:
        if(rt_work_actor.flags&ACT_DRAWN)return VM_DONE;
        r=rt_actor_fix(id,&rt_work_actor);rt_work_actor.flags|=ACT_DRAWN|ACT_UPDATE;break;
    case 0x24: rt_work_actor.flags&=~ACT_DRAWN; break;
    case 0x25: case 0x93: rt_work_actor.x=b; rt_work_actor.y=p[2]; break;
    case 0x26: case 0x94: rt_work_actor.x=rt_vm.vars[b]; rt_work_actor.y=rt_vm.vars[p[2]]; break;
    case 0x27: rt_vm.vars[b]=rt_work_actor.x; rt_vm.vars[p[2]]=rt_work_actor.y; return VM_DONE;
    case 0x28:
        x=(int16_t)rt_work_actor.x+(int8_t)rt_vm.vars[b]; y=(int16_t)rt_work_actor.y+(int8_t)rt_vm.vars[p[2]];
        rt_work_actor.x=x<0 ? 0 : x>159 ? 159 : x; rt_work_actor.y=y<0 ? 0 : y>167 ? 167 : y; break;
    case 0x29: case 0x2a: rt_work_actor.view=op==0x29 ? b : rt_vm.vars[b]; r=select(&rt_work_actor); break;
    case 0x2b: case 0x2c: rt_work_actor.loop=op==0x2b ? b : rt_vm.vars[b]; r=select(&rt_work_actor); break;
    case 0x2d: rt_work_actor.flags|=ACT_FIXED_LOOP; break;
    case 0x2e: rt_work_actor.flags&=~ACT_FIXED_LOOP; break;
    case 0x2f: case 0x30: rt_work_actor.cel=op==0x2f ? b : rt_vm.vars[b]; r=select(&rt_work_actor); break;
    case 0x31: r=select(&rt_work_actor); if(!r) rt_vm.vars[b]=rt_work_cel.cels-1; return r;
    case 0x32: rt_vm.vars[b]=rt_work_actor.cel; return VM_DONE;
    case 0x33: rt_vm.vars[b]=rt_work_actor.loop; return VM_DONE;
    case 0x34: rt_vm.vars[b]=rt_work_actor.view; return VM_DONE;
    case 0x35: r=select(&rt_work_actor); if(!r) rt_vm.vars[b]=rt_work_cel.loops; return r;
    case 0x36: case 0x37: rt_work_actor.priority=op==0x36 ? b : rt_vm.vars[b]; rt_work_actor.flags|=ACT_FIXED_PRIORITY; break;
    case 0x38: rt_work_actor.flags&=~ACT_FIXED_PRIORITY; break;
    case 0x39: rt_vm.vars[b]=rt_work_actor.flags&ACT_FIXED_PRIORITY ? rt_work_actor.priority : agi_view_priority(rt_work_actor.y); return VM_DONE;
    case 0x3a: rt_work_actor.flags&=~ACT_UPDATE; break;
    case 0x3b: rt_work_actor.flags|=ACT_UPDATE; break;
    case 0x3c: rt.dirty|=2; return rt_actor_render();
    case 0x3d: rt_work_actor.flags|=ACT_IGNORE_HORIZON; break;
    case 0x3e: rt_work_actor.flags&=~ACT_IGNORE_HORIZON; break;
    case 0x43: rt_work_actor.flags|=ACT_IGNORE_OBJECTS; break;
    case 0x44: rt_work_actor.flags&=~ACT_IGNORE_OBJECTS; break;
    case 0x40: rt_work_actor.ignore_blocks|=ACT_ON_WATER;break;
    case 0x41: rt_work_actor.ignore_blocks|=ACT_ON_LAND;break;
    case 0x42: rt_work_actor.ignore_blocks&=~(ACT_ON_WATER|ACT_ON_LAND);break;
    case 0x58: rt_work_actor.ignore_blocks|=ACT_IGNORE_BLOCKS;break;
    case 0x59: rt_work_actor.ignore_blocks&=~ACT_IGNORE_BLOCKS;break;
    case 0x45:
        rt_actor_read(b,&other);i=255;
        if((rt_work_actor.flags&ACT_DRAWN) && (other.flags&ACT_DRAWN)) {
            x=(int16_t)rt_work_actor.x+rt_work_actor.width/2-other.x-other.width/2;y=(int16_t)rt_work_actor.y-other.y;
            i=(x<0?-x:x)+(y<0?-y:y);if(i>254)i=254;
        }
        rt_vm.vars[p[2]]=i;return VM_DONE;
    case 0x46: rt_work_actor.flags&=~ACT_CYCLING; break;
    case 0x47: rt_work_actor.flags|=ACT_CYCLING; break;
    case 0x48: rt_work_actor.cycle=0; rt_work_actor.flags|=ACT_CYCLING; break;
    case 0x49: case 0x4b:
        rt_work_actor.cycle=op==0x49 ? 2 : 3; rt_work_actor.cycle_flag=b;
        /* One-shot cycles also restart a stop.update object (SQ1's door). */
        rt_work_actor.flags|=ACT_CYCLING|ACT_UPDATE;
        agi_vm_set_flag(&rt_vm,b,0); break;
    case 0x4a: rt_work_actor.cycle=1; rt_work_actor.flags|=ACT_CYCLING; break;
    case 0x4c: rt_work_actor.cycle_time=rt_work_actor.cycle_count=rt_vm.vars[b]; break;
    case 0x4d: rt_work_actor.direction=rt_work_actor.motion=0; if(!id){rt.player_control=0;rt_vm.vars[6]=0;} break;
    case 0x4e: rt_work_actor.motion=0; if(!id){rt.player_control=1;rt_vm.vars[6]=0;} break;
    case 0x4f: rt_work_actor.step=rt_vm.vars[b]; break;
    case 0x50: rt_work_actor.step_time=rt_work_actor.step_count=rt_vm.vars[b]; break;
    case 0x51: case 0x52:
        rt_work_actor.target_x=op==0x51 ? b : rt_vm.vars[b]; rt_work_actor.target_y=op==0x51 ? p[2] : rt_vm.vars[p[2]];
        rt_work_actor.previous_step=rt_work_actor.step; rt_work_actor.motion=RT_MOTION_MOVE; rt_work_actor.motion_flag=p[4];
        b=op==0x51 ? p[3] : rt_vm.vars[p[3]]; if(b) rt_work_actor.step=b;
        rt_work_actor.flags|=ACT_UPDATE; agi_vm_set_flag(&rt_vm,p[4],0); if(!id) rt.player_control=0;
        rt_motion_direction(id,&rt_work_actor);break;
    case 0x53:
        /* Follow reuses target_x as distance and target_y as detour count.
           A zero distance/step still has a well-defined arrival test. */
        rt_work_actor.motion=RT_MOTION_FOLLOW;
        rt_work_actor.target_x=b>rt_work_actor.step?b:rt_work_actor.step;
        if(!rt_work_actor.target_x)rt_work_actor.target_x=1;
        rt_work_actor.target_y=255;rt_work_actor.motion_flag=p[2];
        rt_work_actor.flags|=ACT_UPDATE;agi_vm_set_flag(&rt_vm,p[2],0);break;
    case 0x54:
        /* Wander retains the shared motion parameter as its countdown,
           including AGI's initial zero -> 255 decrement. */
        rt_work_actor.motion=RT_MOTION_WANDER;rt_work_actor.flags|=ACT_UPDATE;
        if(!id)rt.player_control=0;break;
    case 0x55: rt_work_actor.motion=0; break;
    case 0x84: rt_work_actor.motion=RT_MOTION_NORMAL;rt.player_control=1;break;
    case 0x56: if(rt_vm.vars[b]>8)return VM_ARGUMENT;rt_work_actor.direction=rt_vm.vars[b];if(!id)rt_vm.vars[6]=rt_work_actor.direction;break;
    case 0x57: rt_vm.vars[b]=rt_work_actor.direction; return VM_DONE;
    default: return VM_UNSUPPORTED;
    }
    if(r) return r;
    if(op==0x28 || op==0x93 || op==0x94) {
        r=rt_actor_fix(id,&rt_work_actor);if(r)return r;rt_work_actor.ignore_blocks|=ACT_REPOSITIONED;
    }
    /* position stores byte coordinates, including offscreen staging. draw
       fixes placement; rejecting it here stops SQ1's waterfall room setup. */
    if(rt_work_actor.priority>15) return VM_ARGUMENT;
    rt_actor_write(id,&rt_work_actor); rt.dirty|=1; return VM_DONE;
}
/* Select every actor's current geometry before any collision check. A new
   loop/cel may change its footprint, including a stationary obstacle's. */
static uint8_t animate(void) {
    uint16_t id;uint8_t changed,r,wanted;int16_t dx,dy;
    for(id=0;id<256;++id) {
        if(!rt.occupied[id>>3]){id|=7;continue;}
        if(!(rt.occupied[id>>3]&(1u<<(id&7))))continue;
        rt_actor_read(id,&rt_work_actor);
        if((rt_work_actor.flags&(ACT_ANIMATED|ACT_DRAWN|ACT_UPDATE))!=(ACT_ANIMATED|ACT_DRAWN|ACT_UPDATE))continue;
        changed=0;
        if(!id && !rt_work_actor.motion)rt_work_actor.direction=rt_vm.vars[6];
        if(rt_work_actor.step_count<=1 && rt_work_actor.motion>=RT_MOTION_MOVE)rt_motion_direction(id,&rt_work_actor);
        if(rt_work_actor.step_count<=1 && !(rt_work_actor.flags&ACT_FIXED_LOOP) && (rt_work_actor.direction || rt_work_actor.motion)) {
            dx=dy=0;
            if(rt_work_actor.motion==RT_MOTION_MOVE){dx=(int16_t)rt_work_actor.target_x-rt_work_actor.x;dy=(int16_t)rt_work_actor.target_y-rt_work_actor.y;}
            else {
                if(rt_work_actor.direction>=2 && rt_work_actor.direction<=4)dx=1;
                if(rt_work_actor.direction>=6 && rt_work_actor.direction<=8)dx=-1;
                if(rt_work_actor.direction==1 || rt_work_actor.direction==2 || rt_work_actor.direction==8)dy=-1;
                if(rt_work_actor.direction>=4 && rt_work_actor.direction<=6)dy=1;
            }
            r=select(&rt_work_actor);if(r)return r;wanted=rt_work_actor.loop;
            if(dx>0)wanted=0;else if(dx<0)wanted=1;
            else if(dy && rt_work_cel.loops==4)wanted=dy>0?2:3;
            if(rt_work_cel.loops>=2 && rt_work_cel.loops<=4 && wanted<rt_work_cel.loops && wanted!=rt_work_actor.loop){rt_work_actor.loop=wanted;changed=1;}
        }
        if(rt_work_actor.flags&ACT_CYCLING) {
            if(rt_work_actor.cycle_count>1) --rt_work_actor.cycle_count;
            else {
                rt_work_actor.cycle_count=rt_work_actor.cycle_time ? rt_work_actor.cycle_time : 1;
                r=select(&rt_work_actor); if(r) return r;
                if(rt_work_actor.cycle==1 || rt_work_actor.cycle==3) rt_work_actor.cel=rt_work_actor.cel ? rt_work_actor.cel-1 : rt_work_actor.cycle==1 ? rt_work_cel.cels-1 : 0;
                else if(rt_work_actor.cel+1<rt_work_cel.cels) ++rt_work_actor.cel; else if(!rt_work_actor.cycle) rt_work_actor.cel=0;
                if((rt_work_actor.cycle==2 && rt_work_actor.cel==rt_work_cel.cels-1) || (rt_work_actor.cycle==3 && !rt_work_actor.cel)) {
                    rt_work_actor.flags&=~ACT_CYCLING;
                    rt_work_actor.cycle=rt_work_actor.direction=0;
                    /* Completion is one-shot. start.cycling must not fire the
                       old flag again and continually reset script timers. */
                    agi_vm_set_flag(&rt_vm,rt_work_actor.cycle_flag,1);
                }
                changed=1;
            }
        }
        if(changed) { r=select(&rt_work_actor); if(r) return r; rt.dirty|=1; }
        rt_actor_write(id,&rt_work_actor);
    }
    return VM_DONE;
}
uint8_t rt_actor_tick(void) {
    uint16_t id; uint8_t r,changed,oldx,oldy,border; int16_t dx,dy,x,y;
    r=animate();if(r)return r;rt_motion_begin();
    for(id=0;id<256;++id) {
        if(!rt.occupied[id>>3]){id|=7;continue;}
        if(!(rt.occupied[id>>3]&(1u<<(id&7)))) continue;
        rt_actor_read(id,&rt_work_actor);
        if((rt_work_actor.flags&(ACT_ANIMATED|ACT_DRAWN|ACT_UPDATE))!=(ACT_ANIMATED|ACT_DRAWN|ACT_UPDATE)) continue;
        changed=0;
        if(rt_work_actor.step_count>1) --rt_work_actor.step_count;
        else {
            rt_work_actor.step_count=rt_work_actor.step_time ? rt_work_actor.step_time : 1;
            if(!id && !rt_work_actor.motion) rt_work_actor.direction=rt_vm.vars[6];
            dx=dy=0;
            if(rt_work_actor.motion==RT_MOTION_MOVE) {
                dx=(int16_t)rt_work_actor.target_x-rt_work_actor.x; dy=(int16_t)rt_work_actor.target_y-rt_work_actor.y;
                if(dx>rt_work_actor.step) dx=rt_work_actor.step; if(dx<-(int16_t)rt_work_actor.step) dx=-(int16_t)rt_work_actor.step;
                if(dy>rt_work_actor.step) dy=rt_work_actor.step; if(dy<-(int16_t)rt_work_actor.step) dy=-(int16_t)rt_work_actor.step;
            } else {
                if(rt_work_actor.direction>=2 && rt_work_actor.direction<=4) dx=rt_work_actor.step;
                if(rt_work_actor.direction>=6 && rt_work_actor.direction<=8) dx=-(int16_t)rt_work_actor.step;
                if(rt_work_actor.direction==1 || rt_work_actor.direction==2 || rt_work_actor.direction==8) dy=-(int16_t)rt_work_actor.step;
                if(rt_work_actor.direction>=4 && rt_work_actor.direction<=6) dy=rt_work_actor.step;
            }
            if(rt_work_actor.ignore_blocks&ACT_REPOSITIONED)dx=dy=0;
            oldx=rt_work_actor.x; oldy=rt_work_actor.y; x=(int16_t)rt_work_actor.x+dx; y=(int16_t)rt_work_actor.y+dy;
            if(rt.block_on && !(rt_work_actor.ignore_blocks&ACT_IGNORE_BLOCKS) &&
               ((oldx>rt.block[0] && oldx<rt.block[2] && oldy>rt.block[1] && oldy<rt.block[3]) !=
                (x>rt.block[0] && x<rt.block[2] && y>rt.block[1] && y<rt.block[3]))) {
                x=oldx;y=oldy;rt_work_actor.direction=0;if(!id)rt_vm.vars[6]=0;
            }
            border=0;
            if(x<0){x=0;border=4;}else if(x+rt_work_actor.width>160){x=160-rt_work_actor.width;border=2;}
            if(y+1<rt_work_actor.height){y=rt_work_actor.height-1;border=1;}else if(y>167){y=167;border=3;}
            else if(!(rt_work_actor.flags&ACT_IGNORE_HORIZON) && y<=rt.horizon){y=rt.horizon+1;border=1;}
            if(!rt_actor_valid(id,&rt_work_actor,x,y,oldy)) {
                x=oldx;y=oldy;border=0;r=rt_actor_fix(id,&rt_work_actor);if(r)return r;x=rt_work_actor.x;y=rt_work_actor.y;
            }
            rt_work_actor.x=x; rt_work_actor.y=y; changed=rt_work_actor.x!=oldx || rt_work_actor.y!=oldy;
            /* A wanderer's diagonal can be clipped to a horizontal/vertical
               slide by the horizon or screen edge. Treat that as blocked for
               its next direction decision, even if the other axis advanced.
               Otherwise the byte countdown can keep it sliding for 255 ticks. */
            if(changed && !(border && rt_work_actor.motion==RT_MOTION_WANDER))rt_work_actor.ignore_blocks&=~(ACT_REPOSITIONED|ACT_DIDNT_MOVE);
            else rt_work_actor.ignore_blocks=(rt_work_actor.ignore_blocks&~ACT_REPOSITIONED)|ACT_DIDNT_MOVE;
            if(border){if(!id)rt_vm.vars[2]=border;else{rt_vm.vars[4]=id;rt_vm.vars[5]=border;}}
            if(rt_work_actor.motion==RT_MOTION_MOVE && (border || (rt_work_actor.x==rt_work_actor.target_x && rt_work_actor.y==rt_work_actor.target_y))) {
                agi_vm_set_flag(&rt_vm,rt_work_actor.motion_flag,1); rt_work_actor.motion=rt_work_actor.direction=0; rt_work_actor.step=rt_work_actor.previous_step;
                if(!id){rt.player_control=1;rt_vm.vars[6]=0;}
            }

        }
        if(changed)rt.dirty|=1;
        if(!id)rt_work_actor.ignore_blocks&=~(ACT_ON_WATER|ACT_ON_LAND);
        rt_actor_write(id,&rt_work_actor);
    }
    return VM_DONE;
}
uint8_t rt_actor_test(uint8_t op,const uint8_t *p,uint8_t *value) {
    uint16_t x;
    rt_actor_read(p[0],&rt_work_actor); x=rt_work_actor.x;
    if(op==17) x+=rt_work_actor.width/2;
    if(op==18) x+=rt_work_actor.width ? rt_work_actor.width-1 : 0;
    *value=x>=p[1] && x<=p[3] && rt_work_actor.y>=p[2] && rt_work_actor.y<=p[4];
    if(op==16) *value &= (uint16_t)rt_work_actor.x+rt_work_actor.width<=p[3]+1u;
    return VM_DONE;
}
