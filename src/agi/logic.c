/* ZXQuest banked AGI VM, adapted from Sarien's opcode semantics/tables.
 * Sarien - A Sierra AGI resource interpreter engine
 * Copyright (C) 1999-2001 Stuart George and Claudio Matsuoka
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; see COPYING for further details.
 * The bounded reader, explicit call stack and resumable evaluator are new.
 */
#include <string.h>
#include "logic.h"

static const uint8_t test_args[] = {0,2,2,2,2,2,2,1,1,1,2,5,1,0,0,2,5,5,5};

uint8_t agi_vm_flag(const agi_vm *v, uint8_t f) {
    return (v->flags[f>>3] >> (f&7)) & 1;
}
void agi_vm_set_flag(agi_vm *v, uint8_t f, uint8_t value) {
    uint8_t mask=1u<<(f&7);
    if(value) v->flags[f>>3] |= mask; else v->flags[f>>3] &= ~mask;
}
void agi_vm_reset_scans(agi_vm *v) { memset(v->scan,0,sizeof(v->scan)); }
void agi_vm_init(agi_vm *v, const agi_logic_io *io, agi_command_fn cmd, agi_test_fn test) {
    memset(v,0,sizeof(*v)); v->io=*io; v->command=cmd; v->test=test;
}
void agi_vm_stop(agi_vm *v) {
    while(v->depth) v->io.close(v->frames[--v->depth].handle);
    v->window_size=v->window_pos=0; v->in_if=0; v->status=VM_DONE;
}
static uint8_t finish(agi_vm *v, uint8_t status) {
    agi_vm_stop(v); v->status=status; return status;
}
uint8_t agi_logic_header(const agi_logic_io *io, uint32_t h, uint16_t size, uint16_t *end) {
    uint8_t b[2]; uint16_t length;
    if(size<3) return VM_HEADER;
    if(io->read(h,0,b,2)) return VM_IO;
    length=(uint16_t)b[0] | ((uint16_t)b[1]<<8);
    if(!length || length>size-2) return VM_HEADER;
    *end=length+2; return VM_DONE;
}
static uint8_t push(agi_vm *v, uint8_t number) {
    agi_logic_frame *f; uint8_t r;
    if(v->depth==AGI_VM_DEPTH) return VM_STACK;
    f=&v->frames[v->depth];
    if(v->io.open(number,&f->handle,&f->size)) return VM_IO;
    r=agi_logic_header(&v->io,f->handle,f->size,&f->end);
    f->pc=v->scan[number] ? v->scan[number] : 2;
    if(!r && (f->pc<2 || f->pc>=f->end)) r=VM_BRANCH;
    if(r) { v->io.close(f->handle); return r; }
    f->number=number; ++v->depth;
    v->window_size=v->window_pos=0; return VM_DONE;
}
uint8_t agi_vm_start(agi_vm *v, uint8_t number) {
    uint8_t r;
    agi_vm_stop(v); v->logic=number; v->pc=2; v->opcode=0; v->is_test=0;
    r=push(v,number); v->status=r ? r : VM_RUNNING; return v->status;
}
uint8_t agi_vm_read_code(agi_vm *v, uint16_t at, void *data, uint16_t count) {
    agi_logic_frame *f;
    if(!v->depth) return VM_ARGUMENT;
    f=&v->frames[v->depth-1];
    if(at<2 || at>f->end || count>f->end-at) return VM_TRUNCATED;
    return v->io.read(f->handle,at,data,count) ? VM_IO : VM_DONE;
}
static uint8_t byte(agi_vm *v, agi_logic_frame *f, uint8_t *out) {
    uint16_t n;
    if(v->window_pos==v->window_size) {
        if(f->pc>=f->end) return VM_TRUNCATED;
        n=f->end-f->pc; if(n>AGI_VM_WINDOW) n=AGI_VM_WINDOW;
        v->window_size=v->window_pos=0;
        if(v->io.read(f->handle,f->pc,v->window,n)) return VM_IO;
        v->window_at=f->pc; v->window_size=(uint8_t)n;
    }
    *out=v->window[v->window_pos++]; ++f->pc; return VM_DONE;
}
static uint8_t operands(agi_vm *v, agi_logic_frame *f, uint8_t *p, uint8_t n) {
    uint8_t r;
    if(!n)return VM_DONE;
    /* Refills are code-bounded and control transfers update the cursor.
       Operands already in this frame's window need no resource lookup. */
    if(n<=v->window_size-v->window_pos) {
        memcpy(p,v->window+v->window_pos,n);v->window_pos+=n;f->pc+=n;return VM_DONE;
    }
    while(n--) { r=byte(v,f,p++); if(r) return r; }
    return VM_DONE;
}
static void seek(agi_vm *v, agi_logic_frame *f, uint16_t target) {
    uint16_t offset=target-v->window_at;
    if(target>=v->window_at && offset<=v->window_size)v->window_pos=offset;
    else v->window_size=v->window_pos=0;
    f->pc=target;
}
static uint8_t jump(agi_vm *v, agi_logic_frame *f, int32_t delta) {
    int32_t target=(int32_t)f->pc+delta;
    if(target<2 || target>=f->end) return VM_BRANCH;
    seek(v,f,(uint16_t)target); return VM_DONE;
}
static uint8_t condition(agi_vm *v, agi_logic_frame *f, uint8_t op) {
    uint8_t p[5],r,value=0,active;
    uint16_t at=0,n;
    if(op==0xfd) { v->negate^=1; return VM_DONE; }
    if(op==0xfc) {
        if(v->negate) return VM_TEST;
        if(v->in_or) { v->and_value &= v->or_value; v->in_or=0; }
        else { v->in_or=1; v->or_value=0; }
        return VM_DONE;
    }
    if(op==0xff) {
        if(v->negate || v->in_or) return VM_TEST;
        r=operands(v,f,p,2); if(r) return r;
        n=(uint16_t)p[0] | ((uint16_t)p[1]<<8);
        v->in_if=0;
        /* IF skip lengths are unsigned; goto displacements are signed. */
        return v->and_value ? VM_DONE : jump(v,f,(int32_t)n);
    }
    if(!op || op>18) return VM_TEST;
    if(op==14) {
        r=byte(v,f,p); if(r) return r;
        at=f->pc; n=(uint16_t)p[0]*2;
        if(n>f->end-at) return VM_TRUNCATED;
        seek(v,f,at+n);
    } else { r=operands(v,f,p,test_args[op]); if(r) return r; }
    active=v->and_value && (!v->in_or || !v->or_value);
    if(active) {
        switch(op) {
        case 1: value=v->vars[p[0]]==p[1]; break;
        case 2: value=v->vars[p[0]]==v->vars[p[1]]; break;
        case 3: value=v->vars[p[0]]<p[1]; break;
        case 4: value=v->vars[p[0]]<v->vars[p[1]]; break;
        case 5: value=v->vars[p[0]]>p[1]; break;
        case 6: value=v->vars[p[0]]>v->vars[p[1]]; break;
        case 7: value=agi_vm_flag(v,p[0]); break;
        case 8: value=agi_vm_flag(v,v->vars[p[0]]); break;
        default:
            if(!v->test) return VM_UNSUPPORTED;
            r=v->test(v,op,p,at,&value); if(r) return r;
        }
        value=!!value ^ v->negate;
        if(v->in_or) v->or_value |= value; else v->and_value &= value;
    }
    v->negate=0; return VM_DONE;
}
static uint8_t command(agi_vm *v, agi_logic_frame *f, uint8_t op, const uint8_t *p) {
    uint8_t a=p[0], b=p[1],r; uint32_t h; uint16_t size,end;
    switch(op) {
    case 0:
        v->io.close(f->handle); --v->depth;
        v->window_size=v->window_pos=0; return VM_DONE;
    case 1: if(v->vars[a]!=255) ++v->vars[a]; break;
    case 2: if(v->vars[a]) --v->vars[a]; break;
    case 3: v->vars[a]=b; break;
    case 4: v->vars[a]=v->vars[b]; break;
    case 5: v->vars[a]+=b; break;
    case 6: v->vars[a]+=v->vars[b]; break;
    case 7: v->vars[a]-=b; break;
    case 8: v->vars[a]-=v->vars[b]; break;
    case 9: v->vars[v->vars[a]]=v->vars[b]; break;
    case 10: v->vars[a]=v->vars[v->vars[b]]; break;
    case 11: v->vars[v->vars[a]]=b; break;
    case 12: case 13: case 14:
        agi_vm_set_flag(v,a,op==14 ? !agi_vm_flag(v,a) : op==12); break;
    case 15: case 16: case 17:
        a=v->vars[a]; agi_vm_set_flag(v,a,op==17 ? !agi_vm_flag(v,a) : op==15); break;
    case 18: case 19: v->room=op==18 ? a : v->vars[a]; return VM_ROOM;
    case 20: case 21:
        a=op==20 ? a : v->vars[a];
        if(v->io.open(a,&h,&size)) return VM_IO;
        r=agi_logic_header(&v->io,h,size,&end); v->io.close(h); return r;
    case 22: return push(v,a);
    case 23: return push(v,v->vars[a]);
    case 0x91: v->scan[f->number]=f->pc; break;
    case 0x92: v->scan[f->number]=2; break;
    case 0xa5: v->vars[a]=(uint16_t)v->vars[a]*b; break;
    case 0xa6: v->vars[a]=(uint16_t)v->vars[a]*v->vars[b]; break;
    case 0xa7: case 0xa8:
        if(op==0xa8) b=v->vars[b];
        if(!b) return VM_ARGUMENT;
        v->vars[a]/=b; break;
    default: return v->command ? v->command(v,op,p) : VM_UNSUPPORTED;
    }
    return VM_DONE;
}
uint8_t agi_vm_run(agi_vm *v, uint16_t budget) {
    agi_logic_frame *f; uint8_t op,r,p[7]; uint16_t delta;
    if(v->status!=VM_RUNNING) return v->status;
    f=&v->frames[v->depth-1];
    while(budget--) {
        v->logic=f->number; v->pc=f->pc; v->is_test=v->in_if; v->opcode=0;
        r=byte(v,f,&op); if(r) return finish(v,r);
        v->opcode=op;
        if(v->in_if) r=condition(v,f,op);
        else if(op==0xff) {
            v->in_if=v->and_value=1; v->in_or=v->or_value=v->negate=0; r=0;
        } else if(op==0xfe) {
            r=operands(v,f,p,2);
            if(!r) {
                delta=(uint16_t)p[0] | ((uint16_t)p[1]<<8);
                r=jump(v,f,(int32_t)(int16_t)delta);
            }
        } else if(op>=sizeof(agi_command_args)) r=VM_OPCODE;
        else {
            memset(p,0,sizeof(p));
            r=operands(v,f,p,agi_command_args[op]);
            if(!r) r=command(v,f,op,p);
        }
        if(r==VM_WAIT && !v->is_test) { v->status=VM_WAIT; return VM_WAIT; }
        if(r) return finish(v,r);
        if(!v->depth) return finish(v,VM_DONE);
        /* Only a call or return can change the current frame. */
        if(op==0 || op==22 || op==23)f=&v->frames[v->depth-1];
    }
    return VM_RUNNING;
}
uint8_t agi_vm_resume(agi_vm *v) {
    if(v->status==VM_WAIT && v->depth) v->status=VM_RUNNING;
    return v->status;
}

uint8_t agi_logic_message(const agi_logic_io *io, uint32_t h, uint16_t size,
                          uint8_t number, uint16_t offset, char *text,
                          uint16_t capacity, uint16_t *written) {
    static const char key[]="Avis Durgan";
    uint8_t b[64],r,count,k,c;
    uint16_t end,start,limit,at,rel,n,i;
    if(!text || capacity<2 || !written) return VM_ARGUMENT;
    *written=0; *text=0;
    r=agi_logic_header(io,h,size,&end); if(r) return r;
    if(size-end<3) return VM_MESSAGE;
    if(io->read(h,end,b,3)) return VM_IO;
    count=b[0]; rel=(uint16_t)b[1] | ((uint16_t)b[2]<<8);
    if(!number || number>count) return VM_MESSAGE;
    if((uint16_t)count*2>size-end-3 || !rel || rel>size-end-1) return VM_MESSAGE;
    start=end+3+(uint16_t)count*2; limit=end+1+rel;
    if(limit<start) return VM_MESSAGE;
    if(io->read(h,end+3+(uint16_t)(number-1)*2,b,2)) return VM_IO;
    rel=(uint16_t)b[0] | ((uint16_t)b[1]<<8);
    if(!rel) return offset ? VM_MESSAGE : VM_DONE;
    if(rel<2+(uint16_t)count*2 || rel>=limit-end-1) return VM_MESSAGE;
    at=end+1+rel;
    if(offset>=limit-at) return VM_MESSAGE;
    at+=offset; k=(at-start)%11;
    while(at<limit) {
        n=limit-at; if(n>sizeof(b)) n=sizeof(b);
        if(io->read(h,at,b,n)) return VM_IO;
        for(i=0;i<n;++i) {
            c=b[i]^key[k]; if(++k==11) k=0;
            if(!c) return VM_DONE;
            if(*written==capacity-1) return VM_MORE;
            text[(*written)++]=c; text[*written]=0;
        }
        at+=n;
    }
    return VM_MESSAGE;
}
