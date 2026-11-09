/* Inventory selection semantics adapted from Sarien inv.c.
 * Copyright (C) 1999-2001 Stuart George and Claudio Matsuoka.
 * GNU GPL; see COPYING. Banked names, pagination and item panels are new.
 */
#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "runtime.h"
#include "platform.h"

/* Page 61: parser ends below $6c00. Name offsets/path stay outside modal
   text/scratch. Use indirect pointers to avoid the SDCC fixed-struct bug. */
typedef struct { uint16_t offsets[256],size;uint8_t encrypted;char path[110]; } object_data;
static object_data *objects=(object_data *)0x6c00;
static const char key[]="Avis Durgan";
static uint16_t count,selected;
static uint8_t selectable,more_text;
static uint8_t decrypt(uint8_t *bytes,uint8_t phase) {
    uint8_t i;volatile uint8_t *p=bytes;
    /* -SO3 can drop the original byte in constant-index array ^= expressions.
       Volatile loads here retain the actual encrypted bytes in emitted Z80. */
    for(i=0;i<3;++i){p[i]=p[i]^key[phase];if(++phase==11)phase=0;}
    return phase;
}

uint8_t rt_objects(const char *directory) {
    uint8_t file,b[3],r=VM_DONE,k=0,saved=ZXN_READ_REG(0x53);
    uint16_t size,i,offset;struct esx_stat st;char path[110];
    rt.objects=0;
    if(strlen(directory)>100)return VM_ARGUMENT;
    sprintf(path,"%s/OBJECT",directory);errno=0;
    file=esx_f_open(path,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);if(errno)return VM_IO;
    esx_f_fstat(file,&st);
    if(errno || st.size<3 || st.size>65535UL){r=VM_HEADER;goto done;}
    if(esx_f_read(file,b,3)!=3){r=VM_IO;goto done;}
    ZXN_WRITE_REG(0x53,61);objects->size=st.size;
    size=(uint16_t)b[0]|((uint16_t)b[1]<<8);objects->encrypted=size>768;
    if(objects->encrypted)k=decrypt(b,0);
    size=(uint16_t)b[0]|((uint16_t)b[1]<<8);
    if(size%3 || size>768 || size>st.size-3){r=VM_HEADER;goto done;}
    strcpy(objects->path,path);
    for(i=0;i<size/3;++i) {
        if(esx_f_read(file,b,3)!=3){r=VM_IO;goto done;}
        ZXN_WRITE_REG(0x53,61);
        if(objects->encrypted)k=decrypt(b,k);
        offset=(uint16_t)b[0]|((uint16_t)b[1]<<8);
        if(offset<size || offset>objects->size-4){r=VM_HEADER;goto done;}
        objects->offsets[i]=offset+3;rt.locations[i]=b[2];
    }
    rt.objects=size/3;
done:
    esx_f_close(file);ZXN_WRITE_REG(0x53,saved);return r;
}
uint8_t rt_object_name(uint8_t id,char *out) {
    uint8_t file,r=VM_HEADER,k,c,i,used=0,saved=ZXN_READ_REG(0x53),buffer[64];
    uint16_t at,n;char path[110];
    out[0]=0;if(id>=rt.objects)return VM_ARGUMENT;
    ZXN_WRITE_REG(0x53,61);strcpy(path,objects->path);at=objects->offsets[id];k=at%11;
    errno=0;file=esx_f_open(path,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);
    if(errno){ZXN_WRITE_REG(0x53,saved);return VM_IO;}
    esx_f_seek(file,at,ESX_SEEK_SET);if(errno){r=VM_IO;goto done;}
    while(at<objects->size) {
        n=objects->size-at;if(n>sizeof(buffer))n=sizeof(buffer);
        if(esx_f_read(file,buffer,n)!=n){r=VM_IO;goto done;}
        ZXN_WRITE_REG(0x53,61);
        for(i=0;i<n;++i) {
            c=buffer[i];if(objects->encrypted)c^=key[k];if(++k==11)k=0;
            if(!c){out[used]=0;r=VM_DONE;goto done;}
            /* Continue validating to the terminator even after display clipping. */
            if(used<40)out[used++]=c;
        }
        at+=n;
    }
done:
    out[used]=0;esx_f_close(file);ZXN_WRITE_REG(0x53,saved);return r;
}
static uint8_t item_id(uint16_t index) {
    uint16_t i;
    for(i=0;i<rt.objects;++i)if(rt.locations[i]==255){if(!index)return i;--index;}
    return 255;
}
static void label(uint8_t row,uint8_t col,const char *s) {
    /* Constants live in this overlay; text's call gate must get resident data. */
    strcpy(rt_buffer,s);rt_text_at(row,col,rt_buffer,0);
}
static uint8_t item(uint16_t index,uint8_t highlight) {
    char name[41];uint8_t r,fg=rt.fg,bg=rt.bg;
    r=rt_object_name(item_id(index),name);if(r)return r;
    memset(rt_buffer,' ',19);memcpy(rt_buffer,name,strlen(name)>19?19:strlen(name));rt_buffer[19]=0;
    rt.fg=highlight?15:0;rt.bg=highlight?0:15;
    rt_text_at(2+(index%40)/2,index&1?20:1,rt_buffer,0);
    rt.fg=fg;rt.bg=bg;return VM_DONE;
}
static uint8_t selection_name(void) {
    uint8_t r=0;char name[41];
    label(23,0,"                                        ");
    if(count){r=rt_object_name(item_id(selected),name);if(!r)label(23,0,name);}
    return r;
}
static uint8_t inventory_page(void) {
    uint16_t i,first=(selected/40)*40;uint8_t r=0;
    rt_modal(RT_MODAL_CLEAR);label(0,11,"You are carrying:");
    for(i=first;i<count && i<first+40;++i){r=item(i,selectable && i==selected);if(r)return r;}
    if(!count)label(3,16,"nothing");
    if(count>40){sprintf(rt_buffer,"Page %u/%u",(unsigned)(first/40+1),(unsigned)((count+39)/40));rt_text_at(22,15,rt_buffer,0);}
    label(24,1,selectable?"ENTER: select   Shift+1: cancel":"Any key: return   Up/Down: pages");
    return selection_name();
}
static void close_inventory(void) {
    rt_modal(RT_MODAL_RESTORE);rt.ui=0;rt.key=rt.key_pending=rt_vm.vars[19]=0;
}
typedef struct { int16_t x,top;uint8_t transparent; } item_position;
/* Called through a resident gate by overlay 52. Direct display pixels never
   alter the scene, priority map, actors or permanent text cells. */
void bank_inventory_row(void *context,uint8_t row,const uint8_t *pixels,uint8_t width) {
    item_position *pos=context;int16_t x,y=pos->top+row;uint16_t i;uint8_t c,saved;
    /* The lower inspection panel is 320x104. Oversized cels are clipped. */
    if(y<112 || y>=216)return;
    saved=ZXN_READ_REG(0x53);
    for(i=0;i<width;++i) {
        c=pixels[i];x=pos->x+(int16_t)i*2;
        if(c==pos->transparent || x<0 || x>=320)continue;
        ZXN_WRITE_REG(0x53,40+((uint16_t)x>>5));
        *((uint8_t *)(0x6000u+((uint16_t)(x&31)<<8)+y))=c;
        *((uint8_t *)(0x6100u+((uint16_t)(x&31)<<8)+y))=c;
    }
    ZXN_WRITE_REG(0x53,saved);
}
static uint8_t show_object(uint8_t id) {
    next_view view;agi_view_info info;agi_view_cel cel;item_position pos;
    uint8_t r;uint16_t at,n,i,used=0;
    r=next_view_open(&view,id);if(r)return VM_IO;
    r=agi_view_inspect(&view.source,&info);
    if(!r)r=agi_view_select(&view.source,0,0,&cel);
    if(!r)r=agi_view_decode(&view.source,&cel,0,0);
    if(r){r=VM_HEADER;goto done;}
    rt_buffer[0]=0;at=info.description;
    if(at) {
        /* Bounded, terminated descriptions. Validate before touching pixels. */
        for(;;) {
            if(at>=view.source.length || used==512){r=VM_HEADER;goto done;}
            n=view.source.length-at;if(n>512-used)n=512-used;
            if(resource_copy(view.handle,at,rt_buffer+used,n)){r=VM_IO;goto done;}
            for(i=0;i<n;++i)if(!rt_buffer[used+i])break;
            if(i<n)break;
            used+=n;at+=n;
        }
    }
    r=rt_modal(RT_MODAL_FORMAT);if(r)goto done;
    rt_modal(RT_MODAL_CAPTURE);rt_modal(RT_MODAL_CLEAR);
    pos.x=((160-(int16_t)cel.width)/2)*2;pos.top=216-(int16_t)cel.height;pos.transparent=cel.transparent;
    r=agi_view_decode(&view.source,&cel,rt_inventory_row,&pos);
    if(r){rt_modal(RT_MODAL_RESTORE);r=VM_HEADER;goto done;}
    more_text=rt_modal(RT_MODAL_PAGE);rt.ui=RT_UI_OBJECT;r=VM_WAIT;
done:
    next_view_close(&view);return r;
}
uint8_t rt_inventory(uint8_t op,uint8_t a,uint8_t b) {
    uint16_t i,old=selected;uint8_t r=0,fg=rt.fg,bg=rt.bg;char name[41];
    if(op==0x81 || op==0xa2)return show_object(op==0x81?a:rt_vm.vars[a]);
    rt.fg=0;rt.bg=15;
    if(op==0x7c) {
        count=selected=0;selectable=agi_vm_flag(&rt_vm,13);
        /* Preflight names before capturing or painting a partial modal. */
        for(i=0;i<rt.objects;++i)if(rt.locations[i]==255){r=rt_object_name(i,name);if(r)goto done;++count;}
        rt_modal(RT_MODAL_CAPTURE);rt.ui=RT_UI_INVENTORY;r=inventory_page();
        if(!r)r=VM_WAIT;else close_inventory();
    } else if(rt.ui==RT_UI_OBJECT) {
        if(a==27 || (a==13 && !more_text))close_inventory();
        else if(a==13 || a==' ')more_text=rt_modal(RT_MODAL_PAGE);
    } else if(rt.ui==RT_UI_INVENTORY) {
        if(selectable) {
            if(a==27 || a==13){rt_vm.vars[25]=(a==27 || !count)?255:item_id(selected);close_inventory();goto done;}
            if(b==0x48 && selected>=2)selected-=2;
            if(b==0x50 && selected+2<count)selected+=2;
            if(b==0x4b && (selected&1))--selected;
            if(b==0x4d && !(selected&1) && selected+1<count)++selected;
        } else if(count>40 && (b==0x48 || b==0x50)) {
            if(b==0x48 && selected>=40)selected-=40;
            if(b==0x50 && selected+40<count)selected+=40;
        } else if(a || b){close_inventory();goto done;}
        if(selected!=old) {
            if(old/40!=selected/40)r=inventory_page();
            else {r=item(old,0);if(!r)r=item(selected,1);if(!r)r=selection_name();}
            if(r)close_inventory();
        }
    }
done:
    rt.fg=fg;rt.bg=bg;return r;
}
