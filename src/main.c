#include <input.h>
#include <z80.h>
#include <stdio.h>
#include "agi/picture.h"
#include "next/platform.h"
#include "next/view_resource.h"
#ifndef GAME_DIRECTORY
#define GAME_DIRECTORY "games/sq1"
#endif
static uint8_t resource[4096];
static uint8_t picture=1, priority_view, view_number=1, loop_number, cel_number, visible=1;
static int16_t actor_x=76, actor_y=140;
static next_view actor;
static agi_view_cel cel;
static char status[80];

static void show_status(void) {
    sprintf(status,"PIC %03u  VIEW %03u  L%u C%u  %s    ",(unsigned)picture,(unsigned)view_number,
        (unsigned)loop_number,(unsigned)cel_number,priority_view ? "PRI" : "VIS");
    next_text(1,3,status);
}
static void show_error(uint8_t error) {
    sprintf(status,"RESOURCE/SPRITE ERROR %u              ",(unsigned)error);
    next_text(1,3,status);
}
static void erase_actor(void) {
    next_scene_restore(actor_x,actor_y-cel.height+1,cel.width,cel.height);
}
static uint8_t draw_actor(void) {
    if(!visible) return 0;
    return agi_view_blit(&actor.source,&cel,actor_x,actor_y,agi_view_priority(actor_y));
}
static uint8_t select_cel(void) {
    uint8_t result=agi_view_select(&actor.source,loop_number,cel_number,&cel);
    if(result) return result;
    /* The diagnostic viewer keeps the selected cel within the room where possible. */
    if(actor_x+cel.width>AGI_WIDTH) actor_x=cel.width<AGI_WIDTH ? AGI_WIDTH-cel.width : 0;
    if(actor_y+1<cel.height) actor_y=cel.height<=AGI_HEIGHT ? cel.height-1 : AGI_HEIGHT-1;
    return 0;
}
static uint8_t load_picture(void) {
    uint16_t length;
    resource_handle handle;
    uint8_t result=resource_acquire(RESOURCE_PICTURE,picture,&handle);
    if(result) return result;
    resource_size(handle,&length);
    result=length>sizeof(resource) ? RES_TOO_LARGE : resource_copy(handle,0,resource,length);
    resource_release(handle);
    if(result) return result;
    agi_picture_clear();
    result=agi_picture_decode(resource,length);
    if(result) return result+10;
    next_scene_capture();
    result=draw_actor();
    next_show_picture(priority_view); show_status();
    return result;
}
static uint8_t change_view(int8_t direction) {
    uint8_t attempts=0, candidate=view_number, result;
    resource_handle handle;
    /* Preserve the old pin while locating the next existing resource. */
    do {
        candidate+=direction;
        result=resource_acquire(RESOURCE_VIEW,candidate,&handle);
    } while(result==RES_MISSING && ++attempts);
    if(result) return result;
    resource_release(handle);
    next_view_close(&actor);
    result=next_view_open(&actor,candidate);
    if(result) return result;
    view_number=candidate; loop_number=cel_number=0;
    return select_cel();
}
int main(void) {
    uint8_t result, key, previous=0, attempts, wanted_loop, moving;
    int16_t old_x, old_y;
    uint8_t old_w, old_h;
    next_init();
    next_text(1,27,"WASD MOVE  C CEL  L LOOP  H HIDE");
    next_text(1,28,"V/T VIEW  N/B ROOM  P PRIORITY");
    next_text(1,30,"SPRITE TEST - GAME LOGIC NOT RUNNING");
    result=resource_cache_init(GAME_DIRECTORY);
    if(!result) result=next_view_open(&actor,view_number);
    if(!result) result=select_cel();
    if(!result) result=load_picture();
    if(result) { show_error(result); for(;;) {} }
    for(;;) {
        key=in_inkey();
        if(key>='A' && key<='Z') key+=32;
        if(!key) { previous=0; continue; }
        moving=key=='w' || key=='a' || key=='s' || key=='d';
        if(key==previous && !moving) continue;
        previous=key;
        if(key=='p') { priority_view^=1; next_show_picture(priority_view); show_status(); continue; }
        if(key=='n' || key=='b') {
            attempts=0;
            do {
                picture+=key=='b' ? -1 : 1;
                result=load_picture();
            } while(result==RES_MISSING && ++attempts);
            if(result) show_error(result);
            continue;
        }
        if(!moving && key!='c' && key!='l' && key!='v' && key!='t' && key!='h') continue;
        old_x=actor_x; old_y=actor_y; old_w=cel.width; old_h=cel.height;
        erase_actor(); result=0;
        if(moving) {
            wanted_loop=loop_number;
            if(key=='a') { if(actor_x>0) --actor_x; wanted_loop=1; }
            if(key=='d') { if(actor_x+cel.width<AGI_WIDTH) ++actor_x; wanted_loop=0; }
            if(key=='w') { if(actor_y+1>cel.height) --actor_y; wanted_loop=3; }
            if(key=='s') { if(actor_y<AGI_HEIGHT-1) ++actor_y; wanted_loop=2; }
            if((cel.loops==2 || cel.loops==3 || cel.loops==4) && (wanted_loop<2 || cel.loops==4)) {
                if(wanted_loop!=loop_number) { loop_number=wanted_loop; cel_number=0; }
                else cel_number=(cel_number+1)%cel.cels;
            } else cel_number=(cel_number+1)%cel.cels;
            result=select_cel();
        } else if(key=='c') { cel_number=(cel_number+1)%cel.cels; result=select_cel(); }
        else if(key=='l') { loop_number=(loop_number+1)%cel.loops; cel_number=0; result=select_cel(); }
        else if(key=='h') visible^=1;
        else result=change_view(key=='t' ? -1 : 1);
        if(!result) result=draw_actor();
        /* Upload both old and new rectangles from the final composite. */
        next_show_rect(old_x,old_y-old_h+1,old_w,old_h,priority_view);
        next_show_rect(actor_x,actor_y-cel.height+1,cel.width,cel.height,priority_view);
        if(result) show_error(result); else show_status();
        /* Installed z88dk delay is calibrated for 3.5 MHz; at 28 MHz this
           gives about 40 ms plus rendering. Replace with the engine clock later. */
        if(moving) z80_delay_ms(320);
    }
}
