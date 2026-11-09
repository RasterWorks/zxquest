/* Follow/wander semantics adapted from Sarien motion.c.
 * Copyright (C) 1999-2001 Stuart George and Claudio Matsuoka
 * GNU General Public License; see COPYING.
 * Banked state and bounded random-range selection are ZXQuest adaptations.
 */
#include "runtime.h"

static uint8_t random_below(uint8_t limit) {
    rt.random=rt.random*25173u+13849u;
    return rt.random%limit;
}
static uint8_t axis(int16_t delta,uint8_t distance) {
    return delta<=-(int16_t)distance?0:delta>=distance?2:1;
}
void rt_motion_direction(uint8_t id,rt_actor *a) {
    static const uint8_t directions[9]={8,1,2,7,0,3,6,5,4};
    rt_actor ego;int16_t dx,dy;uint16_t distance;uint8_t direction,previous;
    if(a->motion==RT_MOTION_MOVE) {
        /* Scripted movement must report its direction too. Game logic uses
           ego's V6 to start/stop walking cels even without player control.
           Use the remaining displacement, including a final partial step. */
        dx=(int16_t)a->target_x-a->x;dy=(int16_t)a->target_y-a->y;
        a->direction=directions[axis(dx,1)+3*axis(dy,1)];
        if(!id)rt_vm.vars[6]=a->direction;
        return;
    }
    if(a->motion==RT_MOTION_WANDER) {
        previous=a->target_x--;
        if(!previous || (a->ignore_blocks&ACT_DIDNT_MOVE)) {
            a->direction=random_below(9);
            if(!id)rt_vm.vars[6]=a->direction;
            /* Bounded equivalent of choosing 0..50 until the count is >=6.
               Apply to every actor, including non-ego wanderers. */
            if(a->target_x<6)a->target_x=6+random_below(45);
        }
        return;
    }
    rt_actor_read(0,&ego);
    dx=(int16_t)ego.x+ego.width/2-a->x-a->width/2;
    dy=(int16_t)ego.y-a->y;
    direction=directions[axis(dx,a->target_x)+3*axis(dy,a->target_x)];
    if(!direction) {
        a->direction=0;a->motion=RT_MOTION_NORMAL;
        agi_vm_set_flag(&rt_vm,a->motion_flag,1);
        if(!id)rt_vm.vars[6]=0;
        return;
    }
    if(a->target_y==255)a->target_y=0;
    else if(a->ignore_blocks&ACT_DIDNT_MOVE) {
        a->direction=1+random_below(8);
        distance=((dx<0?-dx:dx)+(dy<0?-dy:dy))/2+1;
        /* No rejection loop, even for step zero or very large step sizes. */
        a->target_y=distance<=a->step?a->step:a->step+random_below(distance-a->step);
        return;
    }
    if(a->target_y)a->target_y=a->target_y>a->step?a->target_y-a->step:0;
    else a->direction=direction;
}
