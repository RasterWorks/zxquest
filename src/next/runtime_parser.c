/* WORDS.TOK/said semantics adapted from Sarien words.c and op_test.c.
 * Copyright (C) 1999-2002 Stuart George and Claudio Matsuoka.
 * GNU GPL; see COPYING. Bounded file reader, banked state and input UI are new.
 */
#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "runtime.h"
#include "platform.h"
#define INPUT_MAX 80
#define WORD_MAX 20
typedef struct {
    uint8_t file; uint16_t size,cache_at,cache_size,index[26];
    uint8_t cache[512];
    char line[INPUT_MAX+1],echo[INPUT_MAX+1],text[INPUT_MAX+1],word[INPUT_MAX+1];
    uint16_t ids[WORD_MAX];uint8_t at[WORD_MAX],length[WORD_MAX],count;
    uint8_t pending,prompt_dirty,status_on,score,sound,gfx;
} parser_state;
typedef char parser_fits_before_object_names[(sizeof(parser_state)<=1024)?1:-1];
/* Text cells occupy $6000..$67ff. Parser state never leaves page 61. */
/* An indirect pointer avoids SDCC's constant-address struct optimization bug. */
static parser_state *state=(parser_state *)0x6800;
#define S state
#define ENTER_DATA uint8_t saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,61)
#define LEAVE_DATA ZXN_WRITE_REG(0x53,saved)
static uint8_t opened;
static uint8_t separator(uint8_t c){return c && !(c>='a'&&c<='z') && !(c>='0'&&c<='9');}
static uint8_t read_byte(uint16_t at,uint8_t *value) {
    uint16_t n,base;
    if(at>=S->size)return VM_TRUNCATED;
    if(!S->cache_size || at<S->cache_at || at-S->cache_at>=S->cache_size) {
        base=at&0xfe00;n=S->size-base;if(n>512)n=512;
        errno=0;esx_f_seek(S->file,base,ESX_SEEK_SET);
        if(errno || esx_f_read(S->file,rt_buffer,n)!=n)return VM_IO;
        ZXN_WRITE_REG(0x53,61);memcpy(S->cache,rt_buffer,n);S->cache_at=base;S->cache_size=n;
    }
    *value=S->cache[at-S->cache_at];return VM_DONE;
}
uint8_t rt_parser_init(const char *directory) {
    uint8_t r=0,i,hi=0,lo=0;uint16_t offset;struct esx_stat st;char path[112];ENTER_DATA;
    if(opened){esx_f_close(S->file);opened=0;}
    memset(S,0,sizeof(*S));
    if(strlen(directory)>100){r=VM_ARGUMENT;goto done;}
    sprintf(path,"%s/WORDS.TOK",directory);errno=0;
    S->file=esx_f_open(path,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);
    if(errno){r=VM_IO;goto done;}opened=1;
    esx_f_fstat(S->file,&st);
    if(errno || st.size<52 || st.size>65535UL){r=VM_HEADER;goto done;}
    S->size=st.size;
    for(i=0;i<26;++i) {
        r=read_byte(2u*i,&hi);if(!r)r=read_byte(2u*i+1,&lo);if(r)goto done;
        offset=((uint16_t)hi<<8)|lo;
        if(offset && (offset<52 || offset>=S->size)){r=VM_HEADER;goto done;}
        S->index[i]=offset;
    }
    S->prompt_dirty=1;
done:
    if(r && opened){esx_f_close(S->file);opened=0;}
    LEAVE_DATA;return r;
}
static uint8_t lookup(const char *input,uint16_t *id,uint8_t *matched) {
    uint16_t at;uint8_t r,c,n,previous=0,hi=0,lo=0;
    *matched=0;*id=19999;
    if(input[0]<'a' || input[0]>'z')return VM_DONE;
    at=S->index[input[0]-'a'];if(!at)return VM_DONE;
    while(at<S->size) {
        r=read_byte(at++,&n);if(r)return r;
        if(at==S->size && !n)break; /* DOS dictionaries may end with a zero. */
        if(n>previous)return VM_HEADER;
        do {
            r=read_byte(at++,&c);if(r)return r;
            if(n==INPUT_MAX)return VM_ARGUMENT;
            S->word[n++]=(c^127)&127;
        }while(!(c&128));
        S->word[n]=0;previous=n;
        r=read_byte(at++,&hi);if(!r)r=read_byte(at++,&lo);if(r)return r;
        /* Avoid an SDCC optimizer crash comparing two directly indexed chars. */
        if(strncmp(S->word,input,1))break;
        if(n>*matched && !strncmp(input,S->word,n) && (!input[n] || separator(input[n]))) {
            *matched=n;*id=((uint16_t)hi<<8)|lo;
        }
    }
    return VM_DONE;
}
static uint8_t lower(uint8_t c){return c>='A'&&c<='Z' ? c+32 : c;}
static uint8_t parse(const char *input) {
    uint8_t at=0,n=0,c,len,r,i;uint16_t id;
    S->count=0;rt_vm.vars[9]=0;agi_vm_set_flag(&rt_vm,2,0);agi_vm_set_flag(&rt_vm,4,0);
    while((c=*input++)!=0) {
        c=lower(c);
        if(c<32)c=' ';
        if(c==' ' && (!n || S->text[n-1]==' '))continue;
        if(n==INPUT_MAX)return VM_ARGUMENT;
        S->text[n++]=c;
    }
    if(n && S->text[n-1]==' ')--n;S->text[n]=0;
    while(at<n) {
        if(separator(S->text[at])){++at;continue;}
        r=lookup(S->text+at,&id,&len);if(r)return r;
        if(!len){for(len=0;S->text[at+len]&&!separator(S->text[at+len]);++len){}id=19999;}
        if(id) {
            i=S->count;if(i==WORD_MAX)return VM_ARGUMENT;
            S->ids[i]=id;S->at[i]=at;S->length[i]=len;S->count=i+1;
            if(id==19999){rt_vm.vars[9]=i+1;break;}
        }
        at+=len;
    }
    agi_vm_set_flag(&rt_vm,2,S->count!=0);return VM_DONE;
}
uint8_t rt_parser_word(uint8_t number,char *out) {
    uint8_t n;ENTER_DATA;
    if(number>=S->count){out[0]=0;LEAVE_DATA;return VM_ARGUMENT;}
    n=S->length[number];if(n>=RT_STRING_SIZE)n=RT_STRING_SIZE-1;
    memcpy(out,S->text+S->at[number],n);out[n]=0;LEAVE_DATA;return VM_DONE;
}
static void prompt(void) {
    uint8_t n,prefix,fg=rt.fg,bg=rt.bg;
    if(!rt.gfx)return;
    memset(rt_buffer,' ',40);rt_buffer[40]=0;
    if(rt.input_enabled) {
    prefix=strlen(rt.strings[0]);if(prefix>38)prefix=38;
    memcpy(rt_buffer,rt.strings[0],prefix);
    n=strlen(S->line);if(n>39-prefix)n=39-prefix;
    memcpy(rt_buffer+prefix,S->line,n);rt_buffer[prefix+n]=rt.cursor ? rt.cursor : '_';
    }
    rt.fg=15;rt.bg=0;rt_text_at(rt.input_line,0,rt_buffer,1);rt.fg=fg;rt.bg=bg;S->prompt_dirty=0;
}
uint8_t rt_parser_key(uint8_t ascii) {
    uint8_t n,limit;ENTER_DATA;
    if(S->pending){LEAVE_DATA;return VM_DONE;}
    n=strlen(S->line);limit=rt_vm.vars[24];if(limit>INPUT_MAX)limit=INPUT_MAX;
    if(ascii==13){if(n){strcpy(S->echo,S->line);S->pending=1;} }
    else if((ascii==8 || ascii==127) && n)S->line[--n]=0;
    else if(ascii>=32 && ascii<127 && n<limit){S->line[n++]=ascii;S->line[n]=0;}
    S->prompt_dirty=1;prompt();LEAVE_DATA;return VM_DONE;
}
void rt_parser_invalidate_display(void) {
    ENTER_DATA;S->status_on=0;S->prompt_dirty=1;S->gfx=rt.gfx;LEAVE_DATA;
}
void rt_parser_reset_input(void) {
    ENTER_DATA;
    S->line[0]=S->echo[0]=S->text[0]=S->count=S->pending=S->status_on=0;
    S->prompt_dirty=1;S->gfx=rt.gfx;
    LEAVE_DATA;
}
uint8_t rt_parser_cycle(uint8_t start) {
    uint8_t r=0,fg,bg,sound;ENTER_DATA;
    if(start) {
        if(S->pending){r=parse(S->line);S->pending=0;S->line[0]=0;S->prompt_dirty=1;}
    } else {
        agi_vm_set_flag(&rt_vm,2,0);agi_vm_set_flag(&rt_vm,4,0);rt_vm.vars[9]=0;
        if(rt.gfx!=S->gfx){S->gfx=rt.gfx;S->prompt_dirty=1;S->status_on=0;}
        if(S->prompt_dirty)prompt();
        sound=agi_vm_flag(&rt_vm,9);
        if(rt.gfx && rt.status_enabled && (!S->status_on || S->score!=rt_vm.vars[3] || S->sound!=sound)) {
            memset(rt_buffer,' ',40);rt_buffer[40]=0;
            sprintf(rt_buffer,"Score: %u of %u",(unsigned)rt_vm.vars[3],(unsigned)rt_vm.vars[7]);
            rt_buffer[strlen(rt_buffer)]=' ';memcpy(rt_buffer+29,sound ? "Sound: on " : "Sound: off",10);
            fg=rt.fg;bg=rt.bg;rt.fg=0;rt.bg=15;rt_text_at(rt.status_line,0,rt_buffer,1);rt.fg=fg;rt.bg=bg;
            S->status_on=1;S->score=rt_vm.vars[3];S->sound=sound;
        }
        if(!rt.status_enabled)S->status_on=0;
    }
    LEAVE_DATA;return r;
}
uint8_t rt_parser_command(uint8_t op,const uint8_t *p) {
    uint8_t r=0;ENTER_DATA;
    switch(op) {
    case 0x75: if(p[0]>=RT_STRINGS)r=VM_ARGUMENT;else r=parse(rt.strings[p[0]]);break;
    case 0x74:
        if(p[0]>=RT_STRINGS)r=VM_ARGUMENT;else r=rt_parser_word(p[1],rt.strings[p[0]]);break;
    case 0x77:
        if(rt.input_enabled) {
            rt.input_enabled=0;S->line[0]=S->pending=0;
            /* Clear only the editable line. The parsed sentence and said
               flags still belong to the current LOGIC scan. */
            prompt();
        }
        break;
    case 0x78:rt.input_enabled=1;S->prompt_dirty=1;prompt();break;
    case 0x89:strcpy(S->line,S->echo);S->prompt_dirty=1;prompt();break;
    case 0x8a:S->line[0]=0;S->prompt_dirty=1;prompt();break;
    default:r=VM_UNSUPPORTED;
    }
    LEAVE_DATA;return r;
}
static uint8_t compare_char(const char **p) {
    uint8_t c;const char *s=*p;
    do{c=*s++;}while(c && (c==' '||c=='\t'||c=='-'||c=='.'||c==','||c==':'||c==';'||c=='!'||c=='\''));
    *p=s;
    return lower(c);
}
uint8_t rt_parser_test(uint8_t op,const uint8_t *p,uint16_t at,uint8_t *value) {
    uint8_t i=0,r=0,b[2],c,d;uint16_t word;const char *s1,*s2;ENTER_DATA;
    *value=0;
    if(op==15) {
        if(p[0]>=RT_STRINGS || p[1]>=RT_STRINGS){r=VM_ARGUMENT;goto done;}
        s1=rt.strings[p[0]];s2=rt.strings[p[1]];
        do{c=compare_char(&s1);d=compare_char(&s2);if(c!=d)goto done;}while(c);
        *value=1;goto done;
    }
    if(!agi_vm_flag(&rt_vm,2) || agi_vm_flag(&rt_vm,4) || rt_vm.vars[9])goto done;
    for(i=0;i<p[0];++i) {
        r=agi_vm_read_code(&rt_vm,at+2u*i,b,2);if(r)goto done;
        word=b[0]|((uint16_t)b[1]<<8);
        if(word==9999){*value=1;break;}
        if(i>=S->count || (word!=1 && word!=S->ids[i]))goto done;
    }
    if(i==p[0] && i==S->count)*value=1;
    if(*value)agi_vm_set_flag(&rt_vm,4,1);
done:
    LEAVE_DATA;return r;
}
