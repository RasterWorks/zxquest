/* Native ZXQuest snapshots. Restore lifecycle follows Sarien savegame.c:
 * restart LOGIC 0 with flag 12, discard input and complete stopped sound.
 * Copyright (C) 1999-2003 Stuart George and Claudio Matsuoka (semantics).
 * GNU GPL; see COPYING. File format, bank staging and transaction are new.
 */
#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "runtime.h"
#include "platform.h"
#include "sound.h"
#include "save.h"
#include "resource_cache_internal.h"

extern void rt_text_mask(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
#define SAVE_VERSION 1
#define VM_BYTES 800
#define SAVE_BYTES (VM_BYTES+(uint32_t)sizeof(rt_state)+8192UL+2048UL+65536UL)
#define STAGE_PAGES ((SAVE_BYTES+8191)/8192)
typedef struct {
    char magic[8];uint16_t version,state_size;
    uint32_t length,game,generation,crc;
    char description[32];
} save_header;
typedef char vm_layout_check[(offsetof(agi_vm,frames)-offsetof(agi_vm,vars)==VM_BYTES)?1:-1];
typedef char header_layout_check[(sizeof(save_header)==60)?1:-1];
typedef char staging_capacity[(STAGE_PAGES<=12)?1:-1];
uint8_t rt_save_identity_valid;
/* Optional UI label; stored only in the existing header description field. */
char rt_save_label[32];
static uint32_t identity;
static uint8_t stage[STAGE_PAGES];
/* Diagnostic phase/error retained for the filesystem regression harness. */
uint8_t rt_save_phase,rt_save_errno;
static const uint32_t crc_table[16]={
    0,0x1db71064UL,0x3b6e20c8UL,0x26d930acUL,0x76dc4190UL,0x6b6b51f4UL,0x4db26158UL,0x5005713cUL,
    0xedb88320UL,0xf00f9344UL,0xd6d6a3e8UL,0xcb61b38cUL,0x9b64c2b0UL,0x86d3d2d4UL,0xa00ae278UL,0xbdbdf21cUL};
static uint32_t crc_bytes(uint32_t crc,const void *data,uint16_t n) {
    const uint8_t *p=data;
    while(n--) {crc^=*p++;crc=(crc>>4)^crc_table[crc&15];crc=(crc>>4)^crc_table[crc&15];}
    return crc;
}
static uint8_t identify(void) {
    static const char *names[]={"LOGDIR","PICDIR","VIEWDIR","SNDDIR","OBJECT","WORDS.TOK"};
    uint8_t i,f;uint16_t n;uint32_t crc=0xffffffffUL;char path[112];
    struct esx_stat st;uint32_t left;
    if(rt_save_identity_valid)return SAVE_OK;
    for(i=0;i<6;++i) {
        sprintf(path,"%s/%s",cache_game_directory,names[i]);errno=0;
        rt_save_phase=10+i;f=esx_f_open(path,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);if(errno)return SAVE_IO;
        esx_f_fstat(f,&st);if(errno){esx_f_close(f);return SAVE_IO;}
        crc=crc_bytes(crc,&st.size,4);left=st.size;
        while(left) {
            n=left>512?512:(uint16_t)left;
            if(esx_f_read(f,rt_buffer,n)!=n){esx_f_close(f);return SAVE_IO;}
            crc=crc_bytes(crc,rt_buffer,n);left-=n;
        }
        esx_f_close(f);
    }
    identity=~crc;rt_save_identity_valid=1;return SAVE_OK;
}
/* Private to overlay 92. UI receives paths/headers in resident storage. */
uint8_t save_path(uint8_t slot,char *path) {
    uint8_t r=identify();if(r)return r;
    sprintf(path,"saves/%04X%04X/%u.a",(unsigned)(identity>>16),(unsigned)identity,(unsigned)slot);
    return SAVE_OK;
}
static uint8_t header_read(uint8_t file,save_header *h) {
    struct esx_stat st;
    if(esx_f_read(file,h,sizeof(*h))!=sizeof(*h))return SAVE_INVALID;
    if(memcmp(h->magic,"ZXQSAVE1",8) || h->version!=SAVE_VERSION || h->state_size!=sizeof(rt_state) || h->length!=SAVE_BYTES || !h->generation || h->description[31])return SAVE_INVALID;
    if(h->game!=identity)return SAVE_GAME;
    esx_f_fstat(file,&st);if(errno)return SAVE_IO;
    return st.size==sizeof(*h)+SAVE_BYTES?SAVE_OK:SAVE_INVALID;
}
/* Snapshots never serialize resource handles, VM stack pointers, file handles,
   code addresses, parser cache, modal backing or IRQ sound state. MMU2 is never
   changed here; mapping pictures through MMU3 keeps platform.c's cache valid. */
static void state_chunk(uint32_t at,uint16_t n,uint8_t apply) {
    uint8_t *p,page=0;uint16_t offset,first;
    if(at<VM_BYTES)p=rt_vm.vars+(uint16_t)at;
    else if((at-=VM_BYTES)<sizeof(rt))p=(uint8_t *)&rt+(uint16_t)at;
    else {
        at-=sizeof(rt);
        if(at<8192){page=60;}
        else if((at-=8192)<2048){page=61;}
        else {at-=2048;page=32+(at>>13);at&=8191;}
        ZXN_WRITE_REG(0x53,page);p=(uint8_t *)(0x6000u+(uint16_t)at);
    }
    if(apply)memcpy(p,rt_buffer,n);
    else {
        memcpy(rt_buffer,p,n);
        /* Cel cache contains session-local handles, not snapshot state.
           Keep the v1 payload length but zero its unused page-39 tail. */
        if(page==39){offset=(uint16_t)p-0x6000u;if(offset+n>0x1900u){first=offset<0x1900u?0x1900u-offset:0;memset(rt_buffer+first,0,n-first);}}
    }
}
static uint16_t chunk_size(uint32_t at) {
    uint32_t left;
    if(at<VM_BYTES)left=VM_BYTES-at;
    else if((at-=VM_BYTES)<sizeof(rt))left=sizeof(rt)-at;
    else if((at-=sizeof(rt))<8192)left=8192-at;
    else if((at-=8192)<2048)left=2048-at;
    else {at-=2048;left=8192-(at&8191);}
    return left>512?512:(uint16_t)left;
}
static void stage_chunk(uint32_t at,uint16_t n,uint8_t write) {
    uint8_t *p,*b=(uint8_t *)rt_buffer;uint16_t k;
    while(n) {
        k=8192-(at&8191);if(k>n)k=n;
        ZXN_WRITE_REG(0x53,stage[at>>13]);p=(uint8_t *)(0x6000u+(uint16_t)(at&8191));
        if(write)memcpy(p,b,k);else memcpy(b,p,k);
        at+=k;b+=k;n-=k;
    }
}
static uint8_t read_snapshot(const char *path,uint8_t staging,uint32_t *generation) {
    uint8_t f,r;uint16_t n;uint32_t at,crc;save_header h;
    errno=0;f=esx_f_open(path,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);
    if(errno)return errno==ESX_ENOENT?SAVE_EMPTY:SAVE_IO;
    r=header_read(f,&h);if(r)goto done;
    crc=crc_bytes(0xffffffffUL,&h,24);crc=crc_bytes(crc,h.description,32);
    for(at=0;at<SAVE_BYTES;at+=n) {
        n=SAVE_BYTES-at>512?512:(uint16_t)(SAVE_BYTES-at);
        if(esx_f_read(f,rt_buffer,n)!=n){r=SAVE_IO;goto done;}
        crc=crc_bytes(crc,rt_buffer,n);if(staging)stage_chunk(at,n,1);
    }
    if(~crc!=h.crc)r=SAVE_INVALID;else *generation=h.generation;
done:
    esx_f_close(f);return r;
}
static uint8_t write_snapshot(const char *path,uint32_t generation) {
    uint8_t f,r=SAVE_IO;uint16_t n;uint32_t at,crc;save_header h;
    memcpy(h.magic,"ZXQSAVE1",8);h.version=SAVE_VERSION;h.state_size=sizeof(rt);
    h.length=SAVE_BYTES;h.game=identity;h.generation=generation;h.crc=0;memset(h.description,0,32);
    sprintf(h.description,"Room %u  Score %u  %02u:%02u",(unsigned)rt_vm.vars[0],(unsigned)rt_vm.vars[3],(unsigned)rt_vm.vars[13],(unsigned)rt_vm.vars[12]);
    if(rt_save_label[0]){strncpy(h.description,rt_save_label,31);h.description[31]=0;}
    crc=crc_bytes(0xffffffffUL,&h,24);crc=crc_bytes(crc,h.description,32);
    rt_save_phase=30;errno=0;f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);if(errno)return SAVE_IO;
    if(esx_f_write(f,&h,sizeof(h))!=sizeof(h))goto done;
    for(at=0;at<SAVE_BYTES;at+=n) {
        n=chunk_size(at);state_chunk(at,n,0);crc=crc_bytes(crc,rt_buffer,n);
        if(esx_f_write(f,rt_buffer,n)!=n)goto done;
    }
    h.crc=~crc;esx_f_seek(f,0,ESX_SEEK_SET);
    if(errno || esx_f_write(f,&h,sizeof(h))!=sizeof(h))goto done;
    rt_save_phase=31;esx_f_sync(f);if(!errno)r=SAVE_OK;
done:
    esx_f_close(f);if(errno)r=SAVE_IO;return r;
}
/* Use alternating generations rather than rename. A failed write can only
   damage the older/inactive copy. CRC covers the generation as well as state. */
static uint8_t commit_save(char *path) {
    uint8_t ra,rb,r;uint32_t a=0,b=0,g;uint16_t end=strlen(path)-1;
    strcpy(rt_buffer,"saves");esx_f_mkdir(rt_buffer);
    strcpy(rt_buffer,path);rt_buffer[end-3]=0;esx_f_mkdir(rt_buffer);
    ra=read_snapshot(path,0,&a);path[end]='b';rb=read_snapshot(path,0,&b);
    if(ra==SAVE_IO || rb==SAVE_IO)return SAVE_IO;
    if(ra)a=0;if(rb)b=0;
    g=a>b?a:b;if(g==0xffffffffUL)return SAVE_INVALID;
    path[end]=!a || (b && b>a)?'a':'b';
    rt_save_phase=40;r=write_snapshot(path,g+1);if(r)return r;
    return read_snapshot(path,0,&g);
}
static uint8_t generation(const char *path,uint32_t *g) {
    uint8_t f,r;save_header h;
    *g=0;errno=0;f=esx_f_open(path,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);
    if(errno)return errno==ESX_ENOENT?SAVE_EMPTY:SAVE_IO;
    r=header_read(f,&h);if(!r)*g=h.generation;esx_f_close(f);return r;
}
static uint8_t restore_stage(char *path) {
    uint32_t a,b,g;uint8_t ra,rb,r;uint16_t end=strlen(path)-1;
    ra=generation(path,&a);path[end]='b';rb=generation(path,&b);
    if(ra && rb)return ra==SAVE_EMPTY?rb:ra;
    path[end]=!rb && (ra || b>a)?'b':'a';
    r=read_snapshot(path,1,&g);
    if(r) {path[end]=path[end]=='a'?'b':'a';if((path[end]=='a'?!ra:!rb))r=read_snapshot(path,1,&g);}
    return r;
}
uint8_t rt_save_description(uint8_t slot,char *out) {
    char path[40];save_header h;uint32_t a,b;uint8_t r,ra,rb,f;
    r=save_path(slot,path);if(r)return r;
    ra=generation(path,&a);path[strlen(path)-1]='b';rb=generation(path,&b);
    if(ra && rb)return ra==SAVE_EMPTY?rb:ra;
    if(rb || (!ra && a>=b))path[strlen(path)-1]='a';
    errno=0;f=esx_f_open(path,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);if(errno)return SAVE_IO;
    r=header_read(f,&h);if(!r)memcpy(out,h.description,32);esx_f_close(f);return r;
}
uint8_t rt_save_slot(uint8_t slot,uint8_t restore) {
    uint8_t r,saved=ZXN_READ_REG(0x53);uint16_t n,random=rt.random;uint32_t at;char path[40];
    if(slot<1 || slot>9)return SAVE_INVALID;
    rt_save_phase=1;r=save_path(slot,path);if(r){rt_save_errno=errno;return r;}
    if(!restore){r=commit_save(path);goto done;}
    if(resource_scratch(STAGE_PAGES,stage)){r=SAVE_MEMORY;goto done;}
    r=restore_stage(path);
    if(!r) {
        /* No file IO beyond this point: even a mid-read failure cannot leave
           half a restored room. The active VM remains pinned until validated. */
        rt_sound_stop();agi_vm_stop(&rt_vm);
        for(at=0;at<SAVE_BYTES;at+=n){n=chunk_size(at);stage_chunk(at,n,0);state_chunk(at,n,1);}
        /* Resource handles are session-local; never reuse serialized cel cache. */
        rt_text_mask(4,0,0,0,0);
        /* Keep the live random stream moving instead of replaying the saved
           future. Clock jitter also varies restores after a fresh launch.
           Retain the serialized field for compatibility with existing saves. */
        rt.random=(random^next_clock())*25173u+13849u;
        rt.ui=rt.error=rt.key=rt.key_pending=0;
        memset(rt.controllers,0,32);memset(rt.pending,0,32);
        rt_vm.vars[9]=rt_vm.vars[19]=0;
        agi_vm_set_flag(&rt_vm,2,0);agi_vm_set_flag(&rt_vm,4,0);agi_vm_set_flag(&rt_vm,12,1);
        if(rt.sound_active)agi_vm_set_flag(&rt_vm,rt.sound_flag,1);
        rt.sound_active=0;rt.sound_ticks=0;rt.elapsed_ms=0;
        next_picture_top=32+rt.print_line*8;rt_parser_reset_input();rt_redraw();rt_parser_cycle(0);
    }
    resource_scratch_release(STAGE_PAGES,stage);
done:
    rt_save_errno=errno;ZXN_WRITE_REG(0x53,saved);return r;
}
