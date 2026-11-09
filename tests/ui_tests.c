#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
#include "next/save.h"
#include "agi/picture.h"
extern uint16_t rt_dialog(uint8_t);
static uint8_t file,checks,failures;
static void progress(uint16_t n){uint8_t f;char path[32];strcpy(path,"build/ui-progress.txt");f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);sprintf(rt_buffer,"checks=%u failures=%u cycle=%u\n",checks,failures,n);esx_f_write(f,rt_buffer,strlen(rt_buffer));esx_f_close(f);}
static void check(uint8_t yes,const char *name){++checks;if(!yes)++failures;sprintf(rt_buffer,"%s %s\n",yes?"PASS":"FAIL",name);esx_f_write(file,rt_buffer,strlen(rt_buffer));}
static void snapshot(const char *name){uint8_t f,p,m=ZXN_READ_REG(0x53);strcpy(rt_buffer,name);f=esx_f_open(rt_buffer,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);for(p=40;p<50;++p){ZXN_WRITE_REG(0x53,p);esx_f_write(f,(void *)0x6000,8192);}esx_f_close(f);ZXN_WRITE_REG(0x53,m);}
static uint16_t checksum(void){uint8_t p,m=ZXN_READ_REG(0x53);uint16_t at,n=0;for(p=40;p<50;++p){ZXN_WRITE_REG(0x53,p);for(at=0;at<8192;++at)n=(n<<1)|(n>>15),n+=*((uint8_t *)(0x6000u+at));}ZXN_WRITE_REG(0x53,m);return n;}
static uint8_t pixel(uint16_t x,uint8_t y){uint8_t m=ZXN_READ_REG(0x53),v;ZXN_WRITE_REG(0x53,40+(x>>5));v=*((uint8_t *)(0x6000u+((x&31)<<8)+y));ZXN_WRITE_REG(0x53,m);return v;}
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
static void snapshot(const char *path);
static uint8_t wrapping(uint8_t file) {
    uint8_t mode,row,col,c,p[7]={0},width,n,rows,top,left,ok=1,pass,m2,m3,m4,fg=rt.fg,bg=rt.bg;
    uint16_t before,actual,expected;const char *s;
    m2=ZXN_READ_REG(0x52);m3=ZXN_READ_REG(0x53);m4=ZXN_READ_REG(0x54);
    strcpy(rt.strings[1],"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    for(mode=0;mode<8;++mode) {
        rt_redraw();before=checksum();rt_vm.logic=11;p[0]=7+mode;
        rt_format(11,p[0],rt_buffer,sizeof(rt_buffer));width=1;n=0;s=rt_buffer;
        while(*s){if(*s=='\n' || *s=='\r')n=0;else if(n<36)++n;if(n>width)width=n;++s;}
        pass=rt_text_command(0x65,p)==VM_WAIT && rt.ui==RT_UI_MESSAGE;
        actual=checksum();
        if(mode==0 || mode==6) {
            strcpy(rt_buffer,mode==0?"build/runtime-wrap.l2":"build/runtime-wrap-long.l2");snapshot(rt_buffer);
        }
        if(rt.fg!=fg || rt.bg!=bg || ZXN_READ_REG(0x52)!=m2 || ZXN_READ_REG(0x53)!=m3 || ZXN_READ_REG(0x54)!=m4)pass=0;
        rt_ui_key(13,0);
        if(rt.ui || checksum()!=before)pass=0;
        rows=1;s=wrapped[mode];while(*s)if(*s++=='\n')++rows;
        top=(25-rows-2)/2;left=(40-width-2)/2;
        rt_frame(left,top,width+2,rows+2,132);
        row=top+1;col=left+1;s=wrapped[mode];
        while((c=*s++)!=0) {
            if(c=='\n'){++row;col=left+1;}
            else next_agi_char(col++,row,c,0,15);
        }
        expected=checksum();if(actual!=expected)pass=0;
        if(!pass)ok=0;
        sprintf(rt_buffer,"wrap %u actual=%04x expected=%04x restore/banks=%s\n",(unsigned)mode,actual,expected,pass?"PASS":"FAIL");
        esx_f_write(file,rt_buffer,strlen(rt_buffer));
    }
    return ok;
}
static void positioned(void){
    char path[20];uint8_t p[7]={9,2,3,36,0,0,0},r;uint16_t before,shown,n;rt_actor a;
    strcpy(path,"games/sq1");check(!rt_init(path),"positioned-dialog SQ1 initialization");
    rt_picture(33,0);rt.picture_visible=1;rt_redraw();before=checksum();rt_vm.logic=33;
    r=rt_command(&rt_vm,0x97,p);shown=checksum();snapshot("build/ui-print-at.l2");
    check(r==VM_WAIT && rt.ui==RT_UI_MESSAGE && pixel(18,50)==4,"print.at dispatches and places its bordered message at row 2 column 3");
    rt_key(13,0);check(!rt.ui && checksum()==before,"positioned message restores exact scene on Enter");
    rt_vm.vars[200]=9;rt_vm.vars[2]=24;rt_vm.vars[3]=39;rt_vm.vars[36]=1;p[0]=200;
    r=rt_command(&rt_vm,0x98,p);check(r==VM_WAIT && checksum()==shown,"print.at.v dereferences only message, not row column or width");
    rt_key(27,0);check(!rt.ui && checksum()==before,"positioned message restores exact scene on Escape");
    p[0]=9;p[1]=255;p[2]=255;p[3]=255;r=rt_command(&rt_vm,0x97,p);
    check(r==VM_WAIT && pixel(2,218)==4,"oversized positioned dialog clamps safely inside viewport");rt_key(13,0);
    p[1]=2;p[2]=3;p[3]=0;r=rt_command(&rt_vm,0x97,p);
    check(r==VM_WAIT && rt.string_limit==0 && pixel(18,50)==4,"print.at zero width uses default wrapping");rt_key(13,0);
    /* Original skimmer room, including the exact reported opcode at PC 189. */
    rt_init(path);p[0]=0;rt_actor_command(0x21,p);rt_vm.vars[0]=33;rt_vm.vars[1]=34;
    r=agi_vm_start(&rt_vm,33);for(n=0;r==VM_RUNNING && n<100;++n)r=agi_vm_run(&rt_vm,200);
    check(r==VM_WAIT && rt.ui==RT_UI_MESSAGE && rt_vm.pc==189 && rt_vm.opcode==0x97,"original skimmer initialization reaches positioned message instead of STOP");
    snapshot("build/ui-skimmer.l2");rt_key(13,0);agi_vm_resume(&rt_vm);
    for(n=0;rt_vm.status==VM_RUNNING && n<100;++n)r=agi_vm_run(&rt_vm,200);
    agi_vm_set_flag(&rt_vm,5,0);
    for(n=0;n<80 && !r;++n){r=rt_actor_tick();if(!r){r=agi_vm_start(&rt_vm,33);while(r==VM_RUNNING)r=agi_vm_run(&rt_vm,200);}if(r==VM_WAIT){rt_key(13,0);agi_vm_resume(&rt_vm);while(rt_vm.status==VM_RUNNING)r=agi_vm_run(&rt_vm,200);}}
    rt_actor_read(0,&a);check(!r && n==80 && a.view==75,"original skimmer continues for 80 updates after dismissal");
}
static uint16_t payout_checksum(void) {
    uint16_t x,n=0;uint8_t y,m=ZXN_READ_REG(0x53);uint8_t *p;
    for(x=104;x<240;++x){ZXN_WRITE_REG(0x53,40+(x>>5));p=(uint8_t *)(0x6000u+((x&31)<<8));for(y=160;y<200;++y)n=(n<<1)|(n>>15),n+=p[y];}
    ZXN_WRITE_REG(0x53,m);return n;
}
static void payout_redraw(void) {
    uint8_t row,col,c,attr,m=ZXN_READ_REG(0x53);uint16_t at;
    for(row=16;row<21;++row)for(col=13;col<30;++col){at=(uint16_t)row*40+col;ZXN_WRITE_REG(0x53,61);c=*((uint8_t *)(0x6000u+at));attr=*((uint8_t *)(0x6400u+at));if(c && c!=' ')next_agi_char(col,row,c,attr&15,attr>>4);}
    ZXN_WRITE_REG(0x53,m);
}
static void gambling(void) {
    char path[32];uint8_t p[7]={0},r,ok=1,m,prev=0,seen[16]={0},count=0,x,y;uint16_t n,before;rt_actor a;
    progress(0);strcpy(path,"games/sq1");check(!rt_init(path),"slot-machine SQ1 initialization");
    rt_vm.logic=75;rt_format(75,60,rt_buffer,sizeof(rt_buffer));
    check(!strcmp(rt_buffer," E4 Bets $1    E6 Bets $2    E8 Bets $3") && strlen(rt_buffer)<=40,"original betting instructions fit one line with E4 E6 E8");
    p[1]=3;p[2]=200;rt.random=0x7341;
    for(n=0;n<256;++n){rt_command(&rt_vm,0x82,p);m=rt_vm.vars[200];if(m>3)ok=0;if(n)seen[prev*4+m]=1;prev=m;}
    for(n=0;n<16;++n)count+=seen[n];
    check(ok && count==16,"four-symbol random range covers every adjacent pair instead of a four-draw cycle");
    p[0]=p[1]=255;rt_command(&rt_vm,0x82,p);check(rt_vm.vars[200]==255,"random inclusive singleton upper bound");
    rt_vm.vars[0]=75;rt_vm.vars[1]=70;rt_vm.vars[124]=100;
    r=agi_vm_start(&rt_vm,75);while(r==VM_RUNNING)r=agi_vm_run(&rt_vm,200);
    check(!r,"original gambling scene initializes without STOP");
    progress(1);
    agi_vm_set_flag(&rt_vm,5,0);ok=1;
    for(n=0;n<220 && !r;++n) {
        if(!(n%20))progress(n);
        r=rt_actor_tick();if(!r)r=rt_actor_render();
        if(!n){
            for(y=120;y<155;++y)for(x=52;x<80;++x)if(pixel((uint16_t)x*2,y+next_picture_top)!=(agi_pixel_get((uint16_t)y*160+x)&15))ok=0;
            check(ok,"blank display padding does not mask original payout symbols");
            snapshot("build/ui-gambling-wins.l2");
        }
        before=payout_checksum();payout_redraw();if(payout_checksum()!=before)ok=0;
        if(!r){r=agi_vm_start(&rt_vm,75);while(r==VM_RUNNING)r=agi_vm_run(&rt_vm,200);}
    }
    check(!r && ok && n==220,"animated gambling payout labels survive every dirty rectangle for 220 cycles");
    rt_actor_render();snapshot("build/ui-gambling.l2");

}
int main(void){
    uint8_t r,p[7]={0},m;uint16_t n,before;char directory[32];
    next_init();next_clock_start();strcpy(rt_buffer,"build/ui-tests.txt");file=esx_f_open(rt_buffer,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    strcpy(directory,"build/runtime-fixtures");check(!rt_init(directory),"fixture initialization");
    do{r=rt_step(5);}while(r==VM_RUNNING);
    before=checksum();rt_vm.logic=11;p[0]=2;r=rt_text_command(0x65,p);snapshot("build/ui-dialog.l2");rt_ui_key(13,0);
    check(r==VM_WAIT && !rt.ui && checksum()==before,"sized dialogue restores exact framebuffer");
    m=ZXN_READ_REG(0x53);strcpy(rt_buffer,"Short message.");n=rt_dialog(1);
    check(n==0x0b0d && pixel(98,122)==4 && pixel(96,120)==15 && ZXN_READ_REG(0x53)==m,"short dialogue dimensions, red border, white margin and MMU preservation");
    snapshot("build/ui-short.l2");
    strcpy(rt_buffer,"You are startled by the sound of an alarm. It is followed by an urgent voice which warns that the ship has been boarded by intruders. It ends abruptly.");rt_dialog(1);snapshot("build/ui-alarm.l2");
    check(wrapping(file),"eight independently laid out wrapped dialogues and exact restoration");
    strcpy(directory,"games/sq1");check(!rt_init(directory),"SQ1 initialization");
    do{r=rt_step(5);}while(r==VM_RUNNING);
    before=checksum();r=rt_text_command(0xa1,p);snapshot("build/ui-menu-file.l2");
    rt_ui_key(0,0x4d);snapshot("build/ui-menu-action.l2");rt_ui_key(0,0x4d);snapshot("build/ui-menu-special.l2");rt_ui_key(27,0);
    check(r==VM_WAIT && !rt.ui && checksum()==before,"bordered menus switch and restore exact framebuffer");
    before=checksum();rt_save_ui(0x7d,0,0);snapshot("build/ui-save-list.l2");
    rt_save_ui(0,0,0x50);snapshot("build/ui-save-moved.l2");rt_save_ui(0,0,0x48);
    rt_save_ui(0,13,0);snapshot("build/ui-save-description.l2");rt_save_ui(0,13,0);snapshot("build/ui-save-confirm.l2");rt_save_ui(0,27,0);
    check(!rt.ui && checksum()==before,"save stages cancel without changing game or files");
    rt_save_ui(0x7e,0,0);snapshot("build/ui-restore-list.l2");
    rt_save_ui(0,0,0x50);snapshot("build/ui-restore-moved.l2");rt_save_ui(0,0,0x48);
    rt_save_ui(0,13,0);snapshot("build/ui-restore-confirm.l2");rt_save_ui(0,27,0);
    check(!rt.ui && checksum()==before,"restore confirmation cancels without changing game");
    rt_vm.logic=92;rt.gfx=0;
    rt_format(92,13,rt_buffer,sizeof(rt_buffer));check(!strcmp(rt_buffer," INV VIDEO shows inventory."),"Help Edit spacing and two-column display margin");
    rt_format(92,14,rt_buffer,sizeof(rt_buffer));check(!strcmp(rt_buffer," BREAK pops up menus."),"Help names the Next BREAK key");
    rt_format(92,16,rt_buffer,sizeof(rt_buffer));check(strstr(rt_buffer,"E5 = EDIT + 5")!=0,"Help explains the EDIT shortcut notation");
    rt_format(92,1,rt_buffer,sizeof(rt_buffer));check(!strcmp(rt_buffer,"              SPACE QUEST"),"SPACE QUEST heading retains original centring");
    rt_format(92,2,rt_buffer,sizeof(rt_buffer));check(!strcmp(rt_buffer,"                 Help"),"Help heading retains original centring");
    rt_text_command(0x6a,p);
    for(n=1;n<=16;++n){p[0]=n+1;p[1]=1;p[2]=n;rt_text_command(0x67,p);}
    snapshot("build/ui-help.l2");
    rt.gfx=1;strcpy(rt.strings[1],"Roger Wilco");
    check(!rt_format(14,19,rt_buffer,sizeof(rt_buffer)) && strstr(rt_buffer,"You`re"),"original landing message keeps authored punctuation bytes");
    next_screen_clear(0);rt_format(14,19,rt_buffer,sizeof(rt_buffer));rt_dialog(1);snapshot("build/ui-apostrophe.l2");
    positioned();gambling();
    sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",(unsigned)checks,(unsigned)failures);esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_close(file);
    for(;;){}
}
