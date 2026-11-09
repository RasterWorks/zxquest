#include <arch/zxn.h>
#include <string.h>
#include <stdio.h>
#include "runtime.h"
#include "platform.h"
#include "save.h"

/* Page 61: $7000..$72ff retains expanded modal text, $7e00..$7fff is
   raw formatting scratch. agi_logic_message stages IO in resident memory
   and restores MMU3 before writing each decoded character here. */
#define RAW_SIZE 512
static char *raw=(char *)0x7e00;
static uint16_t object_text_at;
extern void rt_text_mask(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
/* Bank-private calls: both translation units live in overlay 55. */
extern void next_menu_capture(uint8_t *scratch);
extern void next_menu_restore(uint8_t first,uint8_t last,uint8_t *scratch);
extern void next_message_capture(uint8_t first,uint8_t last,uint8_t *scratch);
extern void next_message_restore(uint8_t first,uint8_t last,uint8_t *scratch);
static uint8_t menu_rows,message_last,message_first;
extern void rt_frame(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
extern uint8_t rt_shake(uint8_t count);
static void cell(uint8_t row,uint8_t col,uint8_t c,uint8_t permanent) {
    uint16_t at=(uint16_t)row*40+col; uint8_t saved;
    if(row>=25 || col>=40) return;
    if(permanent) {
        saved=ZXN_READ_REG(0x53); ZXN_WRITE_REG(0x53,61);
        *((uint8_t *)(0x6000u+at))=c;
        *((uint8_t *)(0x6400u+at))=(rt.bg<<4)|(rt.fg&15);
        ZXN_WRITE_REG(0x53,saved);
        rt_text_mask(1,col,row,0,0);
    }
    next_agi_char(col,row,c,rt.fg,rt.bg);
}
void rt_text_clear(void) {
    uint8_t saved=ZXN_READ_REG(0x53); ZXN_WRITE_REG(0x53,61);
    memset((void *)0x6000,0,2048); ZXN_WRITE_REG(0x53,saved);
    rt_text_mask(0,0,0,0,0);
}
void rt_text_redraw(void) {
    uint8_t row,col,c,attr,saved=ZXN_READ_REG(0x53); uint16_t at;
    for(row=0;row<25;++row) for(col=0;col<40;++col) {
        at=(uint16_t)row*40+col; ZXN_WRITE_REG(0x53,61);
        c=*((uint8_t *)(0x6000u+at)); attr=*((uint8_t *)(0x6400u+at));
        if(c) rt_text_mask(3,col,row,c,attr);
    }
    ZXN_WRITE_REG(0x53,saved);
}
void rt_text_at(uint8_t row,uint8_t col,const char *s,uint8_t permanent) {
    uint8_t c,start=col;
    while((c=*s++)!=0 && row<25) {
        if(c=='\n' || c=='\r') { ++row; col=start; continue; }
        cell(row,col,c,permanent);
        if(++col==40) { ++row; col=0; }
    }
}
static uint8_t message_load(uint8_t logic,uint8_t message,uint16_t *n) {
    resource_handle h;uint16_t size;uint8_t r;
    if(resource_acquire(RESOURCE_LOGIC,logic,&h))return VM_IO;
    resource_size(h,&size);r=agi_logic_message(&next_logic_io,h,size,message,0,raw,RAW_SIZE,n);resource_release(h);
    return r==VM_MORE ? VM_ARGUMENT : r;
}
static uint8_t format_text(uint8_t logic,uint8_t message,char *out,uint16_t capacity) {
    uint16_t n,at=0,pos=0,index,j,resume[4];uint8_t r,c,type,depth=0,parents[4],messages[4],width;
    char number[6],word[RT_STRING_SIZE];const char *value;
    if(capacity<2)return VM_ARGUMENT;
    if(message){r=message_load(logic,message,&n);if(r)return r;}
    else {strcpy(raw,(const char *)0x7000);n=strlen(raw);}
    for(;;) {
        if(at==n) {
            if(!depth)break;
            --depth;logic=parents[depth];message=messages[depth];at=resume[depth];
            if(message){r=message_load(logic,message,&n);if(r)return r;}
            else {strcpy(raw,(const char *)0x7000);n=strlen(raw);}
            continue;
        }
        c=raw[at++];
        if(c=='%') {
            if(at==n)return VM_MESSAGE;type=raw[at++];
            if(type=='%')c='%';
            else {
                index=0;if(at==n || raw[at]<'0' || raw[at]>'9')return VM_MESSAGE;
                while(at<n && raw[at]>='0' && raw[at]<='9') {
                    index=index*10+raw[at++]-'0';if(index>255)return VM_ARGUMENT;
                }
                if(type=='m' || type=='g') {
                    if(depth==4)return VM_MESSAGE;
                    parents[depth]=logic;messages[depth]=message;resume[depth++]=at;
                    if(type=='g')logic=0;message=index;at=0;
                    r=message_load(logic,message,&n);if(r)return r;continue;
                }
                if(type=='s'){if(index>=RT_STRINGS)return VM_ARGUMENT;value=rt.strings[index];}
                else if(type=='w'){if(!index)return VM_ARGUMENT;r=rt_parser_word(index-1,word);if(r)return r;value=word;}
                else if(type=='n'){r=rt_object_name(index,word);if(r)return r;value=word;}
                else if(type=='v'){
                    sprintf(number,"%u",(unsigned)rt_vm.vars[index]);value=number;
                    if(at<n && raw[at]=='|') {
                        ++at;width=0;while(at<n && raw[at]>='0' && raw[at]<='9') {
                            width=width*10+raw[at++]-'0';if(width>40)return VM_ARGUMENT;
                        }
                        for(j=strlen(number);j<width;++j){if(pos==capacity-1)return VM_ARGUMENT;out[pos++]='0';}
                    }
                } else return VM_UNSUPPORTED;
                for(j=0;value[j];++j){if(pos==capacity-1)return VM_ARGUMENT;out[pos++]=value[j];}
                continue;
            }
        }
        if(pos==capacity-1)return VM_ARGUMENT;out[pos++]=c;
    }
    out[pos]=0;return VM_DONE;
}
extern void rt_key_labels(char *,uint16_t);
uint8_t rt_format(uint8_t logic,uint8_t message,char *out,uint16_t capacity) {
    uint8_t r,saved=ZXN_READ_REG(0x53);
    if(!message)return VM_MESSAGE;
    ZXN_WRITE_REG(0x53,61);r=format_text(logic,message,out,capacity);
    ZXN_WRITE_REG(0x53,saved);if(!r)rt_key_labels(out,capacity);return r;
}
static void modal_text(uint8_t save) {
    uint8_t saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,61);
    if(save)memcpy((void *)0x7000,rt_buffer,sizeof(rt_buffer));
    else memcpy(rt_buffer,(const void *)0x7000,sizeof(rt_buffer));
    ZXN_WRITE_REG(0x53,saved);
}
static void clear_lines(uint8_t first,uint8_t last,uint8_t colour,uint8_t permanent) {
    uint8_t row,col,old=rt.bg;
    rt.bg=colour ? 15 : 0;
    if(last>24) last=24;
    for(row=first;row<=last && row<25;++row) for(col=0;col<40;++col) cell(row,col,' ',permanent);
    rt.bg=old;
}
static void menu_close(void) {
    next_menu_restore(0,0,(uint8_t *)rt_buffer);
    if(menu_rows>2) next_menu_restore(1,menu_rows-1,(uint8_t *)rt_buffer);
    menu_rows=0; rt.ui=0; rt.key=rt.key_pending=rt_vm.vars[19]=0;
}
extern uint8_t rt_menu_draw(uint8_t *rows);
static uint8_t menu_show(void) {
    uint8_t r=rt_menu_draw(&menu_rows);
    if(r)menu_close();return r;
}
static void first_item(void) {
    uint8_t i;
    rt.item_selected=255;
    for(i=0;i<rt.item_count;++i) if(rt.items[i].menu==rt.menu_selected && rt.items[i].enabled) { rt.item_selected=i; break; }
}
static uint8_t message_flow(uint8_t draw,uint8_t row,uint8_t bottom,uint16_t *offset) {
    uint8_t col=2,last=row,c; uint16_t n; const char *s=rt_buffer+*offset;
    /* Measure and paint with the same layout so modal backing covers every
       touched row. Leave two columns at each side (36 character lines).
       Wait until the next word before wrapping an exactly full line: a
       following explicit newline must not introduce an extra blank row. */
    while(*s && row<=bottom) {
        c=*s;
        if(c=='\n' || c=='\r') {
            ++s;if(c=='\r' && *s=='\n')++s;
            ++row;col=2;continue;
        }
        if(c==' ') {
            ++s;
            /* Retain authored indentation and spacing, but discard spaces
               beyond the right margin instead of indenting a soft wrap. */
            if(col<38) {
                if(draw)cell(row,col,' ',0);
                ++col;if(row>last)last=row;
            }
            continue;
        }
        for(n=0;s[n] && s[n]!=' ' && s[n]!='\n' && s[n]!='\r';++n){}
        if(col>2 && n>38-col) { ++row;col=2; }
        /* Only a single word longer than the whole line can be split. */
        while(n && row<=bottom) {
            if(col==38) { ++row;col=2;if(row>bottom)break; }
            if(draw)cell(row,col,*s,0);
            ++s;++col;--n;if(row>last)last=row;
        }
    }
    *offset=s-rt_buffer;return last;
}
extern uint16_t rt_dialog(uint8_t draw);
/* One gate shares modal backing and word layout with the inventory overlay.
   PAGE returns 1 while more description text remains, otherwise zero. */
uint8_t rt_modal(uint8_t op) {
    uint8_t fg=rt.fg,bg=rt.bg,r=0,saved;
    switch(op) {
    case RT_MODAL_CAPTURE:next_menu_capture((uint8_t *)rt_buffer);break;
    case RT_MODAL_RESTORE:next_menu_restore(0,24,(uint8_t *)rt_buffer);break;
    case RT_MODAL_CLEAR:clear_lines(0,24,15,0);break;
    case RT_MODAL_FORMAT:
        if(strlen(rt_buffer)>=RAW_SIZE)return VM_ARGUMENT;
        modal_text(1);saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,61);
        r=format_text(rt_vm.logic,0,rt_buffer,sizeof(rt_buffer));ZXN_WRITE_REG(0x53,saved);
        if(!r){modal_text(1);object_text_at=0;}break;
    case RT_MODAL_PAGE:
        modal_text(0);rt.fg=0;rt.bg=15;clear_lines(2,8,15,0);
        message_flow(1,2,8,&object_text_at);r=rt_buffer[object_text_at]!=0;
        clear_lines(24,24,15,0);
        rt_text_at(24,2,r?"ENTER: more text   Shift+1: close":"ENTER or Shift+1: return to game",0);break;
    }
    rt.fg=fg;rt.bg=bg;return r;
}
uint8_t rt_text_command(uint8_t op,const uint8_t *p) {
    uint8_t a=p[0],b=p[1],c=p[2],r=0,i,fg,bg; uint16_t n;
    switch(op) {
    case 0x65: case 0x66: case 0x97: case 0x98:
        r=rt_format(rt_vm.logic,(op==0x66 || op==0x98) ? rt_vm.vars[a] : a,rt_buffer,sizeof(rt_buffer)); if(r) return r;
        /* Only the message operand is indirect in print.at.v. Reuse idle
           string-dialog geometry while this mutually exclusive UI is open. */
        i=op>=0x97 ? 2 : 0;
        if(i){rt.string_row=b;rt.string_col=c;rt.string_limit=p[3];}
        n=rt_dialog(i);message_first=n>>8;message_last=n&255;
        modal_text(1);next_message_capture(message_first,message_last,(uint8_t *)rt_buffer);modal_text(0);
        rt_dialog(i|1);
        rt.ui=RT_UI_MESSAGE; return VM_WAIT;
    case 0x67: case 0x68:
        if(op==0x68) { a=rt_vm.vars[a]; b=rt_vm.vars[b]; c=rt_vm.vars[c]; }
        if(a>=25 || b>=40) return VM_ARGUMENT;
        /* Preserve script order: pending draw/erase happens before display. */
        if(rt.dirty && rt.gfx){r=rt_actor_render();if(r)return r;}
        r=rt_format(rt_vm.logic,c,rt_buffer,sizeof(rt_buffer)); if(r) return r;
        rt_text_at(a,b,rt_buffer,1); return VM_DONE;
    case 0x69:
        if(rt.dirty && rt.gfx){r=rt_actor_render();if(r)return r;}
        clear_lines(a,b,c,1); return VM_DONE;
    case 0x6a:
        rt.key=rt.key_pending=0;
        rt.gfx=0; rt_parser_invalidate_display();
        rt_text_clear(); next_screen_clear(rt.bg); return VM_DONE;
    case 0x6b:
        if(!rt.gfx) {
            /* Text-mode cells cease to exist when switching video modes. */
            rt.gfx=1; rt.picture_visible=rt.picture_valid; rt_text_clear();
            next_screen_clear(0);
            if(rt.picture_valid) next_show_picture(0);
        }
        return VM_DONE;
    case 0x6c:
        r=rt_format(rt_vm.logic,a,rt_buffer,sizeof(rt_buffer)); if(!r) rt.cursor=rt_buffer[0]; return r;
    case 0x6d: rt.fg=a&15; rt.bg=b&15; return VM_DONE;
    case 0x6e:return rt_shake(a);
    case 0x6f:
        if(a>4 || b>=25 || c>=25) return VM_ARGUMENT;
        rt.print_line=a; rt.input_line=b; rt.status_line=c; next_picture_top=32+a*8; return VM_DONE;
    case 0x70: rt.status_enabled=1; return VM_DONE;
    case 0x71: rt.status_enabled=0; clear_lines(rt.status_line,rt.status_line,0,1); return VM_DONE;
    case 0x72:
        if(a>=RT_STRINGS) return VM_ARGUMENT;
        r=rt_format(rt_vm.logic,b,rt_buffer,sizeof(rt_buffer)); if(r) return r;
        strncpy(rt.strings[a],rt_buffer,RT_STRING_SIZE-1); rt.strings[a][RT_STRING_SIZE-1]=0; return VM_DONE;
    case 0x73:
        if(a>=RT_STRINGS || c>=25 || p[3]>=40 || !p[4]) return VM_ARGUMENT;
        r=rt_format(rt_vm.logic,b,rt_buffer,sizeof(rt_buffer)); if(r) return r;
        rt_text_at(c,p[3],rt_buffer,1); n=strlen(rt_buffer)+p[3];
        rt.string_row=c; rt.string_col=n>39 ? 39 : n;
        rt.string_id=a; rt.string_limit=p[4]<40 ? p[4] : 40;
        rt.strings[a][0]=0; rt.ui=RT_UI_STRING; cell(c,rt.string_col,rt.cursor ? rt.cursor : '_',0); return VM_WAIT;
    case 0x9c:
        if(rt.menu_count==RT_MENUS || rt.menus_ready) return VM_ARGUMENT;
        rt.menus[rt.menu_count].logic=rt_vm.logic; rt.menus[rt.menu_count++].message=a; return VM_DONE;
    case 0x9d:
        if(!rt.menu_count || rt.item_count==RT_ITEMS || rt.menus_ready) return VM_ARGUMENT;
        rt.items[rt.item_count].logic=rt_vm.logic; rt.items[rt.item_count].message=a;
        rt.items[rt.item_count].menu=rt.menu_count-1; rt.items[rt.item_count].controller=b;
        rt.items[rt.item_count++].enabled=1; return VM_DONE;
    case 0x9e: rt.menus_ready=1; return VM_DONE;
    case 0x9f: case 0xa0:
        for(i=0;i<rt.item_count;++i) if(rt.items[i].controller==a) rt.items[i].enabled=op==0x9f;
        return VM_DONE;
    case 0xa1:
        if(!rt.menus_ready || !rt.menu_count) return VM_DONE;
        next_menu_capture((uint8_t *)rt_buffer); menu_rows=2;
        rt.menu_selected=0; first_item(); rt.ui=RT_UI_MENU; r=menu_show(); return r ? r : VM_WAIT;
    default: return VM_UNSUPPORTED;
    }
}
uint8_t rt_ui_key(uint8_t ascii,uint8_t scan) {
    uint8_t i,n,controller,changed=0; char *s;
    if(rt.ui==RT_UI_CONTROL)return rt_control(0,ascii,scan);
    if(rt.ui==RT_UI_SAVE)return rt_save_ui(0,ascii,scan);
    if(rt.ui==RT_UI_INVENTORY || rt.ui==RT_UI_OBJECT)return rt_inventory(0,ascii,scan);
    if(rt.ui==RT_UI_MESSAGE) {
        if(ascii==13 || ascii==27) { next_message_restore(message_first,message_last,(uint8_t *)rt_buffer); rt.ui=0; }
        return VM_DONE;
    }
    if(rt.ui==RT_UI_STRING) {
        s=rt.strings[rt.string_id]; n=strlen(s);
        if(ascii==13) { rt.ui=0; cell(rt.string_row,rt.string_col+n,' ',1); return VM_DONE; }
        if((ascii==8 || ascii==127) && n) { s[--n]=0; changed=1; }
        else if(ascii>=32 && ascii<=126 && n<rt.string_limit && rt.string_col+n<39) { s[n++]=ascii; s[n]=0; changed=1; }
        if(changed) {
            rt_text_at(rt.string_row,rt.string_col,s,1);
            cell(rt.string_row,rt.string_col+n,rt.cursor ? rt.cursor : '_',0);
            if(rt.string_col+n+1<40) cell(rt.string_row,rt.string_col+n+1,' ',1);
        }
        return VM_DONE;
    }
    if(rt.ui!=RT_UI_MENU) return VM_DONE;
    if(ascii==27) { menu_close(); return VM_DONE; }
    if(ascii==13 && rt.item_selected<rt.item_count && rt.items[rt.item_selected].enabled) {
        controller=rt.items[rt.item_selected].controller;
        rt.controllers[controller>>3]|=1u<<(controller&7);
        menu_close(); return VM_DONE;
    }
    if(scan==0x4b || scan==0x4d) {
        if(scan==0x4b) rt.menu_selected=rt.menu_selected ? rt.menu_selected-1 : rt.menu_count-1;
        else rt.menu_selected=(rt.menu_selected+1)%rt.menu_count;
        first_item(); return menu_show();
    }
    if(scan==0x48 || scan==0x50) {
        i=rt.item_selected;
        for(n=0;n<rt.item_count;++n) {
            if(scan==0x48) i=i==0 || i==255 ? rt.item_count-1 : i-1;
            else i=(i+1)%rt.item_count;
            if(rt.items[i].menu==rt.menu_selected && rt.items[i].enabled) { rt.item_selected=i; break; }
        }
        return menu_show();
    }
    return VM_DONE;
}
