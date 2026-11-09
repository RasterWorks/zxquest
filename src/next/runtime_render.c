/* Clean-background actor compositor, split from runtime_actor.c.
 * Copyright (C) 1999-2003 Stuart George and Claudio Matsuoka
 * GNU General Public License; see COPYING.
 */
#include <string.h>
#include <arch/zxn.h>
/* VIEW decoder and compositor share page 93; direct calls avoid gates. */
#define agi_view_select bank_agi_view_select
#define agi_view_blit bank_agi_view_blit
#define agi_view_priority bank_agi_view_priority
#include "runtime.h"
#include "platform.h"
#include "agi/picture.h"
extern next_view rt_work_view;
extern agi_view_cel rt_work_cel;
extern rt_actor rt_work_actor;
extern uint8_t rt_profile_on;
uint16_t rt_render_profile[5];
#define order ((uint8_t *)0x6000)
#define sortkey ((uint16_t *)0x6100)
#define affected ((uint8_t *)0x6300)
#define controls ((uint8_t *)0x6400)
#define dirty_rows ((uint8_t *)0x6420)
static int16_t left,top,right,bottom;
/* Visibility masks share the parser/text-data overlay. */
extern void rt_text_mask(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
/* display text lives above the picture. Dirty sprite rectangles must restore
   intersecting text cells too, including cells only partly covered by a blit. */
static void restore_text(void) {
    uint8_t row,col,first,last,c,attr;uint16_t at;
    first=((top<0?0:top)+next_picture_top-32)/8;
    last=((bottom>168?168:bottom)-1+next_picture_top-32)/8;if(last>24)last=24;
    for(row=first;row<=last;++row)for(col=left/4;col<40 && col<=(right-1)/4;++col) {
        at=(uint16_t)row*40+col;ZXN_WRITE_REG(0x53,61);
        c=*((uint8_t *)(0x6000u+at));attr=*((uint8_t *)(0x6400u+at));
        /* Visibility masks preserve authored padding without covering
           pixels subsequently overwritten by a sprite commit. */
        if(c)rt_text_mask(3,col,row,c,attr);
    }
}
/* Vertical gaps between independently moving actors need no restoration or
   upload. Every affected actor contributes its full height, so no cel is split. */
static void include_rows(int16_t first,int16_t end) {
    uint8_t row,a,b;
    if(first<0)first=0;if(end>168)end=168;if(end<=first)return;
    --end;
    for(row=first>>3;row<=end>>3;++row){a=row==(first>>3)?first&7:0;b=row==(end>>3)?end&7:7;dirty_rows[row]|=(255u<<a)&(255u>>(7-b));}
}
static uint8_t row_dirty(uint8_t y){return dirty_rows[y>>3]&(1u<<(y&7));}
static void include_rect(uint8_t x,uint8_t y,uint8_t w,uint8_t h) {
    int16_t t=(int16_t)y-h+1;
    if(!w || !h) return;
    include_rows(t,(int16_t)y+1);
    if(x<left) left=x; if(t<top) top=t;
    if((int16_t)x+w>right) right=x+w; if((int16_t)y+1>bottom) bottom=y+1;
}
uint8_t rt_actor_render(void) {
    uint16_t id,n=0,i,j,key,started=0; uint8_t r=0,prio,saved,changed,expand,x,y,band,end,minrow,maxrow,bands;int16_t first,last;
    if(!rt.picture_valid) return VM_DONE;
    saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,91);
    memset(affected,0,256);memset(dirty_rows,0,21);
    if(rt.dirty&2) {
        /* Control pixels look down their column for effective priority. A
           changed sprite can affect another sprite along that ray. */
        memset(controls,0,20);
        for(y=0;y<168;++y) {
            agi_pixels_read((uint16_t)y*160,rt_buffer,160);
            for(x=0;x<160;++x)if((uint8_t)rt_buffer[x]<0x30)controls[x>>3]|=1u<<(x&7);
        }
    }
    left=160; top=168; right=bottom=0;
    for(id=0;id<256;++id) {
        if(!rt.occupied[id>>3]){id|=7;continue;}
        if(!(rt.occupied[id>>3]&(1u<<(id&7)))) continue;
        rt_actor_read(id,&rt_work_actor);
        prio=rt_work_actor.flags&ACT_FIXED_PRIORITY ? rt_work_actor.priority : agi_view_priority(rt_work_actor.y);
        changed=!!(rt_work_actor.flags&ACT_DRAWN)!=rt_work_actor.old_drawn;
        if((rt_work_actor.flags&ACT_DRAWN) && rt_work_actor.old_drawn &&
           (rt_work_actor.x!=rt_work_actor.old_x || rt_work_actor.y!=rt_work_actor.old_y || rt_work_actor.width!=rt_work_actor.old_w || rt_work_actor.height!=rt_work_actor.old_h ||
            rt_work_actor.view!=rt_work_actor.old_view || rt_work_actor.loop!=rt_work_actor.old_loop || rt_work_actor.cel!=rt_work_actor.old_cel || prio!=rt_work_actor.old_priority))changed=1;
        if(changed) {
            if(rt_work_actor.old_drawn)rt_text_mask(2,rt_work_actor.old_x,rt_work_actor.old_y,rt_work_actor.old_w,rt_work_actor.old_h);
            if(rt_work_actor.flags&ACT_DRAWN)rt_text_mask(2,rt_work_actor.x,rt_work_actor.y,rt_work_actor.width,rt_work_actor.height);
        }
        if(changed || (rt.dirty&2)) {
            if(rt_work_actor.old_drawn)include_rect(rt_work_actor.old_x,rt_work_actor.old_y,rt_work_actor.old_w,rt_work_actor.old_h);
            if(rt_work_actor.flags&ACT_DRAWN)include_rect(rt_work_actor.x,rt_work_actor.y,rt_work_actor.width,rt_work_actor.height);
        }
        if(rt_work_actor.flags&ACT_DRAWN) {
            key=((uint16_t)prio<<8)|rt_work_actor.y; j=n;
            while(j && sortkey[j-1]>key) { order[j]=order[j-1]; sortkey[j]=sortkey[j-1]; --j; }
            order[j]=id; sortkey[j]=key; ++n;
        }
        if(!(rt_work_actor.flags&ACT_DRAWN) && rt_work_actor.old_drawn){rt_work_actor.old_drawn=0;rt_actor_write(id,&rt_work_actor);}
    }
    if(right<=left || bottom<=top){rt.dirty=0;goto done;}
    /* Expand to whole intersecting actors before clearing. This retains
       transparency/priority ordering without drawing beyond the dirty area. */
    do {
        expand=0;
        for(i=0;i<n;++i)if(!affected[i]) {
            rt_actor_read(order[i],&rt_work_actor);
            if(rt_work_actor.x<right && rt_work_actor.x+rt_work_actor.width>left && rt_work_actor.y>=top && (int16_t)rt_work_actor.y-rt_work_actor.height+1<bottom) {
                affected[i]=1;expand=1;include_rect(rt_work_actor.x,rt_work_actor.y,rt_work_actor.width,rt_work_actor.height);
            }
        }
        /* A control run depends only on its first normal pixel below it.
           Extend through runs touching this rectangle, not the whole column. */
        for(j=left;j<right && j<160;++j)if(controls[j>>3]&(1u<<(j&7))) {
            while(top>0 && top<=168 && agi_pixel_get((uint16_t)(top-1)*160+j)<0x30){--top;expand=1;}
            while(bottom>0 && bottom<168 && agi_pixel_get((uint16_t)(bottom-1)*160+j)<0x30){++bottom;expand=1;}
        }
    }while(expand);
    minrow=top<0?0:top;maxrow=bottom>168?168:bottom;bands=0;x=0;
    for(y=minrow;y<maxrow;++y){if(row_dirty(y)){if(!x)++bands;x=1;}else x=0;}
    if(bands<=1)include_rows(top,bottom);
    /* Preserve dependencies at every band edge, including internal gaps.
       Control runs use the first normal priority pixel below them. */
    if(bands>1)do {
        expand=0;
        for(band=minrow;band<maxrow;band=end) {
            if(!row_dirty(band)){end=band+1;continue;}
            for(end=band+1;end<maxrow && row_dirty(end);++end){}
            first=band;last=end;
            for(j=left;j<right && j<160;++j)if(controls[j>>3]&(1u<<(j&7))) {
                while(first>0 && agi_pixel_get((uint16_t)(first-1)*160+j)<0x30)--first;
                while(last<168 && agi_pixel_get((uint16_t)(last-1)*160+j)<0x30)++last;
            }
            if(first!=band || last!=end){include_rows(first,last);expand=1;}
        }
    }while(expand);
    for(band=minrow;band<maxrow;band=end) {
        if(!row_dirty(band)){end=band+1;continue;}
        for(end=band+1;end<maxrow && row_dirty(end);++end){}
        top=band;bottom=end;
        if(rt_profile_on){started=next_clock();rt_render_profile[3]+=right-left;rt_render_profile[4]+=bottom-top;}
        next_scene_restore(left,top,right-left,bottom-top);
        if(rt_profile_on){rt_render_profile[0]+=next_clock()-started;started=next_clock();}
        for(i=0;i<n;++i) {
            if(!affected[i])continue;
            id=order[i];rt_actor_read(id,&rt_work_actor);
            if(rt_work_actor.y<top || (int16_t)rt_work_actor.y-rt_work_actor.height+1>=bottom)continue;
            r=next_view_open(&rt_work_view,rt_work_actor.view);if(r){r=VM_IO;goto done;}
            r=agi_view_select(&rt_work_view.source,rt_work_actor.loop,rt_work_actor.cel,&rt_work_cel);
            if(!r)r=agi_view_blit(&rt_work_view.source,&rt_work_cel,rt_work_actor.x,rt_work_actor.y,sortkey[i]>>8);
            next_view_close(&rt_work_view);if(r){r=VM_HEADER;goto done;}
            rt_work_actor.old_x=rt_work_actor.x;rt_work_actor.old_y=rt_work_actor.y;rt_work_actor.old_w=rt_work_cel.width;rt_work_actor.old_h=rt_work_cel.height;rt_work_actor.old_drawn=1;
            rt_work_actor.old_view=rt_work_actor.view;rt_work_actor.old_loop=rt_work_actor.loop;rt_work_actor.old_cel=rt_work_actor.cel;rt_work_actor.old_priority=sortkey[i]>>8;
            rt_actor_write(id,&rt_work_actor);
        }
        if(rt_profile_on){rt_render_profile[1]+=next_clock()-started;started=next_clock();}
        if(rt.gfx && rt.picture_visible){next_show_rect(left,top,right-left,bottom-top,0);restore_text();ZXN_WRITE_REG(0x53,91);}
        if(rt_profile_on)rt_render_profile[2]+=next_clock()-started;
    }
    rt.dirty=0;
done:
    ZXN_WRITE_REG(0x53,saved);return r;
}

/* Shake presentation using Layer 2 scroll, leaving picture, priority and text
   backing untouched. Four presentation ticks per AGI shake count (Sarien). */
uint8_t rt_shake(uint8_t count) {
    uint8_t x=ZXN_READ_REG(0x16),y=ZXN_READ_REG(0x17),hi=ZXN_READ_REG(0x71);
    uint16_t i,n=(uint16_t)count*4,at;
    for(i=0;i<n;++i) {
        ZXN_WRITE_REG(0x16,(i&1)?x:x+4);ZXN_WRITE_REG(0x17,(i&1)?y:y+4);
        at=next_clock();while(next_clock()==at){}
    }
    ZXN_WRITE_REG(0x16,x);ZXN_WRITE_REG(0x17,y);ZXN_WRITE_REG(0x71,hi);
    return VM_DONE;
}
