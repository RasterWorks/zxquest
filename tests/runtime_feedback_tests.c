#include <arch/zxn.h>
#include <intrinsic.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
static uint16_t details[18];
static uint8_t detail_at;
/* This toolchain's -SO3 drops some |= stores to a stack-local bitmask.
   Keep the accumulated test result in explicit storage. */
static uint8_t results;
static uint8_t feedback_ok;
static uint8_t intro_seen;
static const uint8_t probe_y[]={0,31,32,39,40,47,48,55,56,63,64,71,72,215,231,232,255};
/* Distinct pixels that reconstruction from the picture/text state would lose.
   Outside the menu, values above 15 also catch accidental 4-bit conversion. */
static uint8_t probes(uint8_t write,uint8_t menu) {
    uint8_t side,i,y,value,ok=1,saved=ZXN_READ_REG(0x53); uint16_t at;
    for(side=0;side<2;++side) {
        ZXN_WRITE_REG(0x53,side ? 49 : 40);
        for(i=0;i<sizeof(probe_y);++i) {
            y=probe_y[i]; at=0x6000u+(side ? 31u*256 : 0)+y;
            value=y>=32 && y<80 ? (i+side)&15 : 0xa0+i+side;
            if(write) *((uint8_t *)at)=value;
            else if(!(menu && ((y>=32 && y<40) || (y>=40 && y<(menu==1 ? 80 : 64))))) {
                if(*((uint8_t *)at)!=value) ok=0;
            }
        }
    }
    ZXN_WRITE_REG(0x53,saved); return ok;
}
static const uint16_t crc_table[16]={0,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
    0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef};
/* Compare all 80 KB, including the menu bar above the picture and footer. */
static uint16_t frame_crc(void) {
    uint16_t at,crc=0xffff; uint8_t page,b,saved=ZXN_READ_REG(0x53);
    for(page=40;page<50;++page) {
        ZXN_WRITE_REG(0x53,page);
        for(at=0;at<8192;++at) {
            b=*((uint8_t *)(0x6000u+at));
            crc=(crc<<4)^crc_table[(crc>>12)^(b>>4)];
            crc=(crc<<4)^crc_table[(crc>>12)^(b&15)];
        }
    }
    ZXN_WRITE_REG(0x53,saved); return crc;
}
static void save_frame(const char *path);
static uint8_t menu_restore(uint8_t enter) {
    uint8_t p[7]={0},m2,m3,m4; uint16_t before,after;
    rt_redraw(); probes(1,0); before=frame_crc();
    m2=ZXN_READ_REG(0x52);m3=ZXN_READ_REG(0x53);m4=ZXN_READ_REG(0x54);
    if(rt_text_command(0xa1,p)!=VM_WAIT) return 0;
    if(!enter)save_frame("build/runtime-menu.l2");
    if(!probes(0,1)) feedback_ok=0;
    rt_ui_key(0,0x50); if(!probes(0,1)) feedback_ok=0;
    rt_ui_key(0,0x4d); if(!probes(0,2)) feedback_ok=0;
    rt_ui_key(0,0x4b); if(!probes(0,1)) feedback_ok=0;
    rt_ui_key(enter ? 13 : 27,0);
    if(!probes(0,0) || ZXN_READ_REG(0x52)!=m2 || ZXN_READ_REG(0x53)!=m3 || ZXN_READ_REG(0x54)!=m4) feedback_ok=0;
    after=frame_crc();details[detail_at++]=before;details[detail_at++]=after;
    return !rt.ui && after==before;
}
static uint8_t text_blank(uint8_t first,uint8_t last) {
    uint8_t saved=ZXN_READ_REG(0x53),c,ok=1;uint16_t at;
    ZXN_WRITE_REG(0x53,61);
    for(at=(uint16_t)first*40;at<(uint16_t)(last+1)*40;++at) {
        c=*((uint8_t *)(0x6000u+at));if(c && c!=' ')ok=0;
    }
    ZXN_WRITE_REG(0x53,saved);return ok;
}
static uint8_t transitions(void) {
    uint8_t p[7]={0},ok=1;uint16_t expected;
    rt_actor_reset(1);rt.picture_visible=1;rt.gfx=1;rt.bg=0;
    rt_text_clear();rt_redraw();expected=frame_crc();
    rt_text_command(0x6a,p);strcpy(rt_buffer,"OLD STORY");rt_text_at(5,2,rt_buffer,1);
    rt_text_command(0x6b,p);
    if(!text_blank(0,24) || frame_crc()!=expected)ok=0;
    /* graphics while already in graphics mode must not erase current text. */
    strcpy(rt_buffer,"CURRENT");rt_text_at(5,2,rt_buffer,1);expected=frame_crc();
    rt_text_command(0x6b,p);if(frame_crc()!=expected)ok=0;
    /* show.pic replaces text inside its 21 rows, retaining status/footer. */
    rt_text_clear();rt_redraw();strcpy(rt_buffer,"HUD");rt_text_at(0,0,rt_buffer,1);rt_text_at(24,0,rt_buffer,1);
    expected=frame_crc();strcpy(rt_buffer,"OVER PICTURE");rt_text_at(5,2,rt_buffer,1);
    if(rt_command(&rt_vm,0x1a,p) || !text_blank(1,21) || frame_crc()!=expected)ok=0;
    rt_redraw();if(frame_crc()!=expected)ok=0;
    return ok;
}
/* Deliberately include pixels absent from the picture/text model: closing a
   dialogue must restore captured pixels, not reconstruct the whole screen. */
static uint8_t dialogues(uint8_t file) {
    uint8_t p[7]={0},mode,page,saved3=ZXN_READ_REG(0x53),saved4=ZXN_READ_REG(0x54),saved2=ZXN_READ_REG(0x52),ok=1,r;
    uint16_t before,after,start,elapsed;
    for(mode=0;mode<6;++mode) {
        rt.gfx=mode!=1;rt.bg=mode==1?1:0;next_picture_top=mode==2?64:40;
        rt_redraw();probes(1,0);
        /* Long boxes and their footer cover row 22; the clipped message
           also covers row 24. Packed backing uses the game's 16 colours. */
        if(mode>=3) for(page=40;page<=49;page+=9) {
            ZXN_WRITE_REG(0x53,page);
            *((uint8_t *)(page==40?0x60d7:0x7fd7))&=15;
            if(mode==4)*((uint8_t *)(page==40?0x60e7:0x7fe7))&=15;
        }
        /* A visible pixel inside the box: short messages save full bytes;
           long messages pack the normal 16-colour game framebuffer. */
        for(page=40;page<50;++page){ZXN_WRITE_REG(0x53,page);*((uint8_t *)0x6090)=mode<3?0xa7:7;}
        ZXN_WRITE_REG(0x53,saved3);before=frame_crc();
        rt_vm.logic=11;p[0]=mode<3?2:mode==3?4:mode==4?5:6;
        if(mode==1){rt_vm.vars[200]=p[0];p[0]=200;}
        r=rt_text_command(mode==1?0x66:0x65,p);
        if(r!=VM_WAIT || rt.ui!=RT_UI_MESSAGE || frame_crc()==before)ok=0;
        /* An unrecognised key must leave the suspended dialogue visible. */
        rt_ui_key('x',0);if(rt.ui!=RT_UI_MESSAGE)ok=0;
        start=next_clock();rt_ui_key(mode&1?27:13,0);elapsed=next_clock()-start;after=frame_crc();
        if(rt.ui || after!=before || ZXN_READ_REG(0x53)!=saved3 || ZXN_READ_REG(0x54)!=saved4 || ZXN_READ_REG(0x52)!=saved2)ok=0;
        sprintf(rt_buffer,"dialogue %u CRC before=%04x after=%04x close=%u frames\n",(unsigned)mode,before,after,elapsed);
        esx_f_write(file,rt_buffer,strlen(rt_buffer));
    }
    next_picture_top=40;rt.gfx=1;rt.bg=0;
    return ok;
}
/* Hand-laid-out expectations, independent of the runtime's word scanner.
   Compare every framebuffer byte, including blank margins and prompt rows. */
extern void rt_frame(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
static const char *const wrapped[]={
    "You think you hear footsteps\napproaching from the passage behind\nyou.",
    "123456789012345678901234567890123456\nNext line\n\nNew paragraph",
    "Prefix\nWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW\nWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW\nWWWW end",
    "Welcome ABCDEFGHIJKLMNOPQRSTUVWXYZ!\nYou think you hear footsteps\napproaching from the passage behind\nyou.",
    "  Indented text stays indented.\nA  pair   of spaces.\nLast line.",
    "",
    "Line\nLine\nLine\nLine\nLine\nLine\nLine\nLine\nLine\nLine\nThese words must stay visible below\nthe old footer.",
    "123456789012345678901234567890123456\nnext\nend"
};
static void save_frame(const char *path);
static uint8_t wrapping(uint8_t file) {
    uint8_t mode,row,col,c,p[7]={0},width,n,rows,top,left,ok=1,pass,m2,m3,m4,fg=rt.fg,bg=rt.bg;
    uint16_t before,actual,expected;const char *s;
    m2=ZXN_READ_REG(0x52);m3=ZXN_READ_REG(0x53);m4=ZXN_READ_REG(0x54);
    strcpy(rt.strings[1],"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    for(mode=0;mode<8;++mode) {
        rt_redraw();before=frame_crc();rt_vm.logic=11;p[0]=7+mode;
        rt_format(11,p[0],rt_buffer,sizeof(rt_buffer));width=1;n=0;s=rt_buffer;
        while(*s){if(*s=='\n' || *s=='\r')n=0;else if(n<36)++n;if(n>width)width=n;++s;}
        pass=rt_text_command(0x65,p)==VM_WAIT && rt.ui==RT_UI_MESSAGE;
        actual=frame_crc();
        if(mode==0 || mode==6) {
            strcpy(rt_buffer,mode==0?"build/runtime-wrap.l2":"build/runtime-wrap-long.l2");save_frame(rt_buffer);
        }
        if(rt.fg!=fg || rt.bg!=bg || ZXN_READ_REG(0x52)!=m2 || ZXN_READ_REG(0x53)!=m3 || ZXN_READ_REG(0x54)!=m4)pass=0;
        rt_ui_key(13,0);
        if(rt.ui || frame_crc()!=before)pass=0;
        rows=1;s=wrapped[mode];while(*s)if(*s++=='\n')++rows;
        top=(25-rows-2)/2;left=(40-width-2)/2;
        rt_frame(left,top,width+2,rows+2,132);
        row=top+1;col=left+1;s=wrapped[mode];
        while((c=*s++)!=0) {
            if(c=='\n'){++row;col=left+1;}
            else next_agi_char(col++,row,c,0,15);
        }
        expected=frame_crc();if(actual!=expected)pass=0;
        if(!pass)ok=0;
        sprintf(rt_buffer,"wrap %u actual=%04x expected=%04x restore/banks=%s\n",(unsigned)mode,actual,expected,pass?"PASS":"FAIL");
        esx_f_write(file,rt_buffer,strlen(rt_buffer));
    }
    return ok;
}
static void save_frame(const char *path) {
    uint8_t f,page,saved=ZXN_READ_REG(0x53);
    f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    for(page=40;page<50;++page) {ZXN_WRITE_REG(0x53,page);esx_f_write(f,(void *)0x6000,8192);}
    esx_f_close(f);ZXN_WRITE_REG(0x53,saved);
}
static uint8_t intro(uint8_t log) {
    char directory[16];uint8_t r=0,was_graphics=1,clean=1;uint16_t cycle,n,before,after;
    strcpy(directory,"games/sq1");intro_seen=0;
    if(rt_init(directory))return 0;
    for(cycle=0;cycle<2500;++cycle) {
        n=0;do {r=rt_step(n ? 0 : 5);++n;}while(r==VM_RUNNING && n<500);
        if(r || n==500)break;
        if(!rt.gfx) {
            if(rt_vm.vars[230]==16)intro_seen|=1;
            if(rt_vm.vars[230]==32)intro_seen|=2;
            if(rt_vm.vars[230]==47 && !(intro_seen&4)) {
                intro_seen|=4;strcpy(rt_buffer,"build/runtime-intro-story.l2");save_frame(rt_buffer);
            }
        }
        if(!was_graphics && rt.gfx) {
            before=frame_crc();if(!text_blank(0,24))clean=0;
            rt_text_clear();rt_redraw();after=frame_crc();if(before!=after)clean=0;
            sprintf(rt_buffer,"intro graphics return at cycle %u CRC=%04x clean=%04x\n",cycle,before,after);
            esx_f_write(log,rt_buffer,strlen(rt_buffer));intro_seen|=8;
        }
        was_graphics=rt.gfx;
        if((intro_seen&8) && rt.gfx && rt_vm.vars[33]==2) {
            if(!text_blank(0,24))clean=0;
            strcpy(rt_buffer,"build/runtime-intro-credits.l2");save_frame(rt_buffer);break;
        }
    }
    sprintf(rt_buffer,"intro cycles=%u stages=%u clean=%u result=%u room=%u logic=%u pc=%u op=%02x\n",cycle,(unsigned)intro_seen,(unsigned)clean,(unsigned)r,(unsigned)rt_vm.vars[0],(unsigned)rt_vm.logic,rt_vm.pc,(unsigned)rt_vm.opcode);
    esx_f_write(log,rt_buffer,strlen(rt_buffer));
    return cycle<2500 && intro_seen==15 && clean && !r;
}
extern uint8_t next_key_decode(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
static uint8_t prefix_cancel(void) {
    uint8_t ok;
    intrinsic_di();
    next_key_decode(0,0,1,255,255,0);
    ok=next_key_decode(7,0,0,254,255,0)==7;
    next_key_decode(0,0,0,255,255,0);
    ok=ok && next_key_decode('1',0,0,254,255,0)=='1';
    next_key_decode(0,0,1,255,255,0);
    ok=ok && next_key_decode(0,0,32,254,255,0)==7;
    next_key_decode(0,0,1,255,255,0);
    ok=ok && next_key_decode(8,2,0,239,255,0)==8;
    next_key_decode(0,0,0,255,255,0);
    intrinsic_ei();
    return ok;
}
uint8_t bank_runtime_feedback_tests(void) {
    uint8_t i,ok=1,k1,k2,k3,f;
    detail_at=results=0; feedback_ok=prefix_cancel();
    /* feedback_ok also checks prefix cancellation before menu bank/pixel checks. */
    /* The preceding lifecycle test enters a new room, invalidating its old
       picture. Load an explicit fixture for graphics/menu restoration. */
    if(rt_picture(0,0))return 0;
    rt.picture_visible=1;
    /* Exercise switching between a three-item menu and a shorter menu. */
    rt.menus[1]=rt.menus[0]; rt.menu_count=2;
    rt.items[3]=rt.items[0];rt.items[3].menu=1;rt.item_count=4;
    next_picture_top=40;
    if(menu_restore(0) && menu_restore(1)) results|=1;
    next_picture_top=32; ok=menu_restore(0);
    next_picture_top=64; ok&=menu_restore(0);
    if(ok) results|=2;
    next_picture_top=40; rt.gfx=0; rt.bg=1;
    if(menu_restore(0)) results|=4;
    if(feedback_ok) results|=32;
    rt.menu_count=1;rt.item_count=3;
    rt.gfx=1;rt.bg=0;
    /* Produce press/hold/release edges while no foreground polling occurs. */
    while(next_key()) {}
    intrinsic_di();
    next_key_sample(0);next_key_sample(7);next_key_sample(7);
    next_key_sample(0);next_key_sample(8);next_key_sample(0);
    intrinsic_ei(); rt_redraw();
    k1=next_key();k2=next_key();k3=next_key();details[10]=k1;details[11]=k2;details[12]=k3;
    if(k1==7 && k2==8 && !k3) results|=8;
    intrinsic_di();
    for(i=1;i<=40;++i){next_key_sample(i);next_key_sample(0);}
    intrinsic_ei();ok=1;
    for(i=1;i<=31;++i) {k1=next_key();if(k1!=i){ok=0;details[13]=i;details[14]=k1;}}
    if(next_key())ok=0;
    intrinsic_di();next_key_sample(99);next_key_sample(0);intrinsic_ei();
    k1=next_key();k2=next_key();details[15]=k1;details[16]=k2;
    if(ok && k1==99 && !k2)results|=16;
    details[17]=results;
    f=esx_f_open("build/runtime-feedback.txt",ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    for(i=0;i<5;++i){sprintf(rt_buffer,"menu %u CRC before=%04x after=%04x\n",(unsigned)i,details[2*i],details[2*i+1]);esx_f_write(f,rt_buffer,strlen(rt_buffer));}
    sprintf(rt_buffer,"keys=%u,%u,%u overflow mismatch=%u:%u reused=%u,%u result=%u\n",details[10],details[11],details[12],details[13],details[14],details[15],details[16],details[17]);
    esx_f_write(f,rt_buffer,strlen(rt_buffer));
    ok=transitions();if(!dialogues(f))ok=0;if(!wrapping(f))ok=0;
    if(ok)results|=64;
    if(intro(f))results|=128;
    esx_f_close(f);
    return results;
}
