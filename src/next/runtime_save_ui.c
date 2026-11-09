#include <stdio.h>
#include <string.h>
#include <arch/zxn.h>
#include "runtime.h"
#include "platform.h"
#include "save.h"
static uint8_t restore_mode,slot,confirmation,notice;
extern uint8_t rt_save_description(uint8_t slot,char *description);
extern char rt_save_label[32];
/* Nine labels use otherwise unused modal scratch in page 61, not resident RAM.
   Read once when opening; cursor movement never rereads all nine files. */
static void cached(uint8_t id,char *text,uint8_t write) {
    uint8_t saved=ZXN_READ_REG(0x53);char *p=(char *)(0x7400u+(uint16_t)(id-1)*32);
    ZXN_WRITE_REG(0x53,61);
    if(write)memcpy(p,text,32);else memcpy(text,p,32);
    ZXN_WRITE_REG(0x53,saved);
}
static void label(uint8_t row,const char *text) {
    strcpy(rt_buffer,text);rt_text_at(row,2,rt_buffer,0);
}
extern void rt_frame(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
static void edit_line(void) {
    rt.fg=15;rt.bg=0;
    sprintf(rt_buffer,"%-32s",rt_save_label);rt_text_at(12,3,rt_buffer,0);
    strcpy(rt_buffer,"_");rt_text_at(12,3+strlen(rt_save_label),rt_buffer,0);
}
static void panel(void) {
    uint8_t fg=rt.fg,bg=rt.bg,i;char description[32];
    rt_modal(RT_MODAL_RESTORE);rt.fg=0;rt.bg=15;
    if(notice) {
        rt_frame(1,8,38,9,132);
        switch(notice-1) {
        case SAVE_EMPTY:label(10,"This slot is empty.");break;
        case SAVE_INVALID:label(10,"Damaged or incompatible save file.");break;
        case SAVE_GAME:label(10,"Save belongs to a different game.");break;
        case SAVE_MEMORY:label(10,"Not enough free memory to restore.");break;
        default:label(10,"Could not read or write the save.");break;
        }
        label(14,"ENTER or BREAK: return to game");
    } else if(confirmation==1 && !restore_mode) {
        rt_frame(1,8,38,7,132);
        label(9,"How would you like to describe");
        label(10,"this saved game?");edit_line();
    } else if(confirmation) {
        rt_frame(1,7,38,9,132);
        label(8,restore_mode?"About to restore the game":"About to save the game");
        label(9,"described as:");label(11,rt_save_label);
        label(13,"Press ENTER to continue.");label(14,"Press BREAK to cancel.");
    } else {
        rt_frame(1,2,38,17,132);
        label(3,restore_mode?"RESTORE GAME":"SAVE GAME");
        label(5,"Use arrows to select a slot.");
        label(6,"ENTER: select    BREAK: cancel");
        for(i=1;i<=9;++i) {
            cached(i,description,0);
            sprintf(rt_buffer,"%c- %-31s",i==slot?'>':' ',description);
            rt_text_at(i+7,2,rt_buffer,0);
        }
    }
    rt.fg=fg;rt.bg=bg;
}
static void close_panel(void) {
    rt_modal(RT_MODAL_RESTORE);rt.ui=rt.key=rt.key_pending=rt_vm.vars[19]=0;
    rt_save_label[0]=0;
}
uint8_t rt_save_ui(uint8_t op,uint8_t ascii,uint8_t scan) {
    uint8_t r,i,fg,bg,n;char description[32];
    if(op) {
        restore_mode=op==0x7e;slot=1;confirmation=notice=0;rt_save_label[0]=0;
        rt_modal(RT_MODAL_CAPTURE);rt.ui=RT_UI_SAVE;
        for(i=1;i<=9;++i) {
            memset(description,0,32);r=rt_save_description(i,description);
            if(r && r!=SAVE_EMPTY)strcpy(description,"[Unavailable]");
            cached(i,description,1);
        }
        panel();return VM_WAIT;
    }
    if(ascii==27 || (notice && ascii==13)){close_panel();return VM_DONE;}
    if(notice)return VM_DONE;
    if(ascii==13) {
        if(!confirmation){
            confirmation=restore_mode?2:1;cached(slot,rt_save_label,0);
            if(rt_save_label[0]=='[')rt_save_label[0]=0;
            panel();return VM_DONE;
        }
        if(confirmation==1){confirmation=2;panel();return VM_DONE;}
        {
            /* Show the live scene while synchronous IO runs. This transient
               label is not permanent text and never enters the snapshot. */
            rt_modal(RT_MODAL_RESTORE);fg=rt.fg;bg=rt.bg;rt.fg=0;rt.bg=15;
            rt_frame(12,10,16,5,132);
            strcpy(rt_buffer,restore_mode?" Restoring... ":" Saving... ");
            rt_text_at(12,(40-strlen(rt_buffer))/2,rt_buffer,0);rt.fg=fg;rt.bg=bg;
        }
        r=rt_save_slot(slot,restore_mode);rt_save_label[0]=0;
        if(!r) {if(!restore_mode)close_panel();return VM_DONE;}
        notice=r+1;panel();return VM_DONE;
    }
    if(confirmation==1) {
        n=strlen(rt_save_label);
        if((ascii==8 || ascii==127) && n)rt_save_label[--n]=0;
        else if(ascii>=32 && ascii<=126 && n<31){rt_save_label[n++]=ascii;rt_save_label[n]=0;}
        else return VM_DONE;
        fg=rt.fg;bg=rt.bg;edit_line();rt.fg=fg;rt.bg=bg;
    } else if(!confirmation) {
        i=slot;
        if(ascii>='1' && ascii<='9')slot=ascii-'0'; /* compatibility shortcut */
        else if(scan==0x48)slot=slot==1?9:slot-1;
        else if(scan==0x50)slot=slot==9?1:slot+1;
        else return VM_DONE;
        if(slot!=i){next_agi_char(2,i+7,' ',0,15);next_agi_char(2,slot+7,'>',0,15);}
    }
    return VM_DONE;
}
