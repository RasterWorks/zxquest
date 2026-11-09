#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
static uint8_t file,failures,checks,p[7];
static char path[40],line[140];
static void log(const char *s){esx_f_write(file,(void *)s,strlen(s));}
static void check(uint8_t yes,const char *name){++checks;if(!yes)++failures;sprintf(line,"%s %s\n",yes?"PASS":"FAIL",name);log(line);}
static uint8_t cmd(uint8_t op,uint8_t a,uint8_t b){memset(p,0,sizeof(p));p[0]=a;p[1]=b;return rt_command(&rt_vm,op,p);}
static uint8_t parse(const char *s){strncpy(rt.strings[2],s,40);rt.strings[2][40]=0;return cmd(0x75,2,0);}
static uint8_t script(uint8_t n){uint8_t r=agi_vm_start(&rt_vm,n);uint16_t i=0;while(r==VM_RUNNING && i++<100)r=agi_vm_run(&rt_vm,100);return r;}
static uint8_t scan(void){uint8_t r;uint16_t i=0;do{r=rt_step(i?0:5);++i;}while(r==VM_RUNNING && i<500);return i==500?VM_ARGUMENT:r;}
static void sentence(const char *s){while(*s)rt_key(*s++,0);rt_key(13,0);}
static void screen(const char *name){uint8_t f,page,saved=ZXN_READ_REG(0x53);strcpy(path,name);f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);for(page=40;page<50;++page){ZXN_WRITE_REG(0x53,page);esx_f_write(f,(void *)0x6000,8192);}esx_f_close(f);ZXN_WRITE_REG(0x53,saved);}
extern uint8_t rt_profile_on;
extern uint16_t rt_profile[4];
extern uint16_t rt_render_profile[5];
static uint8_t performance(uint8_t direction){
    uint8_t i=0,r=0,attempt=0;uint16_t start,elapsed=0,before[4];
    rt_vm.vars[6]=direction;memset(rt_profile,0,8);memset(rt_render_profile,0,10);rt_profile_on=1;
    while(i<16 && !r && ++attempt<40) {
        memcpy(before,rt_profile,8);start=next_clock();r=scan();
        if(r==VM_WAIT && rt.ui==RT_UI_MESSAGE) {
            /* Let original timed announcements run, but exclude modal drawing
               and dismissal from the movement CPU benchmark. */
            rt_profile_on=0;rt_key(13,0);r=scan();rt_profile_on=1;memcpy(rt_profile,before,8);
        } else if(!r){elapsed+=next_clock()-start;++i;}
    }
    rt_profile_on=0;rt_vm.vars[6]=0;if(i!=16 && !r)r=VM_ARGUMENT;
    sprintf(line,"PERF dir=%u scans=%u total=%u vm=%u actors=%u render=%u input=%u frames\n",(unsigned)direction,(unsigned)i,elapsed,rt_profile[0],rt_profile[1],rt_profile[2],rt_profile[3]);log(line);
    sprintf(line,"RENDER restore=%u draw=%u upload=%u width_sum=%u height_sum=%u\n",rt_render_profile[0],rt_render_profile[1],rt_render_profile[2],rt_render_profile[3],rt_render_profile[4]);log(line);
    return r;
}
static uint8_t input_death_profile(void) {
    uint8_t r,death=rt_vm.vars[130],counter=rt_vm.vars[230];
    /* Controlled comparisons, not an alien encounter: keep the same room
       and actors while toggling input and the original settled-death path. */
    r=scan();if(r)return r;
    log("COMPARE alive input=on\n");r=performance(0);if(r)return r;
    r=cmd(0x77,0,0);if(r)return r;
    log("COMPARE alive input=off\n");r=performance(0);if(r)return r;
    r=cmd(0x78,0,0);if(r)return r;
    log("COMPARE alive input=on repeat\n");r=performance(0);if(r)return r;
    /* SQ1 LOGIC 0 routes v130>0 to LOGIC 100 and returns before room logic.
       v130=8 is its settled state; v230=0 suppresses the one-off epitaph. */
    rt_vm.vars[130]=8;rt_vm.vars[230]=0;
    log("COMPARE settled death input=on same actors\n");r=performance(0);
    rt_vm.vars[130]=death;rt_vm.vars[230]=counter;
    return r;
}
static void authored(void){
    uint8_t r,value,saved;
    strcpy(path,"build/runtime-fixtures");check(!rt_init(path),"authored dictionary opens");
    saved=ZXN_READ_REG(0x53);r=parse("  EXAMINE, the DOOR! ");
    check(!r&&agi_vm_flag(&rt_vm,2)&&!rt_vm.vars[9]&&ZXN_READ_REG(0x53)==saved,"normalization ignored words and bank preservation");
    check(!script(6)&&rt_vm.vars[205]&&agi_vm_flag(&rt_vm,4),"said matches synonyms and consumes input");
    check(!script(6)&&!rt_vm.vars[205],"said cannot consume same sentence twice");
    parse("look door");check(!script(7)&&rt_vm.vars[205],"said anyword matches one word");
    parse("look");check(!script(8)&&rt_vm.vars[205],"said rest-of-line permits empty suffix");
    parse("look door open");check(!script(8)&&rt_vm.vars[205],"said rest-of-line consumes suffix");
    parse("look door");check(!script(9)&&!rt_vm.vars[205],"said rejects unmatched trailing words");
    check(!script(6)&&rt_vm.vars[205],"failed said leaves input available to later patterns");
    parse("pick up");check(!script(10)&&rt_vm.vars[205],"longest dictionary phrase wins");
    parse("look gibberish");check(rt_vm.vars[9]==2&&!script(8)&&!rt_vm.vars[205],"unknown word index and said rejection");
    check(!cmd(0x74,3,1)&&!strcmp(rt.strings[3],"gibberish"),"word.to.string copies parsed unknown word");
    check(!rt_format(11,1,rt_buffer,sizeof(rt_buffer))&&!strcmp(rt_buffer,"Unknown \"gibberish\""),"nested messages and parsed-word substitution");
    check(rt_format(11,3,rt_buffer,sizeof(rt_buffer))==VM_MESSAGE,"recursive message expansion is bounded");
    parse("the a");check(!agi_vm_flag(&rt_vm,2)&&!rt_vm.vars[9],"ignored-only sentence has no input event");
    strcpy(rt.strings[2],"Roger-Wilco!");strcpy(rt.strings[3]," roger wilco ");p[0]=2;p[1]=3;
    check(!rt_test(&rt_vm,15,p,0,&value)&&value,"compare.strings ignores case punctuation and spaces");
    cmd(0x78,0,0);rt_key('l',0);rt_key('o',0);rt_key('x',0);rt_key(8,0);rt_key('o',0);rt_key('k',0);rt_key(13,0);
    r=rt_parser_cycle(1);check(!r&&!script(9)&&rt_vm.vars[205],"command line edits and submits on next scan");
    rt_parser_cycle(0);check(!agi_vm_flag(&rt_vm,2)&&!agi_vm_flag(&rt_vm,4),"input flags expire after completed scan");
    cmd(0x89,0,0);rt_key(13,0);rt_parser_cycle(1);check(!script(9)&&rt_vm.vars[205],"echo.line recalls previous command");
    rt_parser_cycle(0);cmd(0x89,0,0);cmd(0x8a,0,0);rt_key(13,0);rt_parser_cycle(1);check(!agi_vm_flag(&rt_vm,2),"cancel.line clears editable command");
}
static void blocks(void){
    rt_actor a;uint8_t r,saved=ZXN_READ_REG(0x53);
    memset(&a,0,sizeof(a));a.x=10;a.y=50;a.view=2;a.width=a.height=1;
    a.step=a.step_time=a.step_count=1;a.direction=3;
    a.flags=ACT_ANIMATED|ACT_DRAWN|ACT_UPDATE|ACT_FIXED_LOOP;rt_actor_write(1,&a);
    p[0]=10;p[1]=40;p[2]=20;p[3]=60;r=rt_command(&rt_vm,0x5a,p);
    if(!r)r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&a.x==10&&!a.direction,"block rectangle prevents entering its interior");
    a.direction=3;rt_actor_write(1,&a);
    r=cmd(0x58,1,0);if(!r)r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&a.x==11&&(a.ignore_blocks&ACT_IGNORE_BLOCKS),"ignore.blocks permits crossing");
    a.x=19;rt_actor_write(1,&a);r=cmd(0x59,1,0);if(!r)r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&a.x==19&&!(a.ignore_blocks&ACT_IGNORE_BLOCKS),"observe.blocks prevents leaving rectangle");
    a.direction=3;rt_actor_write(1,&a);
    r=cmd(0x5b,0,0);if(!r)r=rt_actor_tick();rt_actor_read(1,&a);
    check(!r&&a.x==20&&ZXN_READ_REG(0x53)==saved,"unblock restores movement and preserves data bank");
}
static void game(void){
    uint8_t r;uint16_t i,start;rt_actor ego;
    strcpy(path,"games/sq1");r=rt_init(path);check(!r,"original SQ1 dictionary opens");if(r)return;
    r=scan();if(r)goto stop;rt_key(' ',0);r=scan();
    check(r==VM_WAIT&&rt.ui==RT_UI_STRING,"original name prompt reached");if(rt.ui!=RT_UI_STRING)goto stop;
    sentence("Roger");r=scan();
    for(i=0;i<100 && !r && !rt.player_control;++i)r=scan();
    rt_actor_read(0,&ego);
    check(!r&&rt_vm.vars[0]==2&&rt.input_enabled&&rt.player_control&&(ego.flags&ACT_DRAWN),"name parse enters playable room 2");if(r)goto stop;
    screen("build/runtime-room2.l2");start=next_clock();sentence("look");r=scan();
    sprintf(line,"look r=%u room=%u logic=%u pc=%u op=%02x frames=%u\n",(unsigned)r,(unsigned)rt_vm.vars[0],(unsigned)rt_vm.logic,rt_vm.pc,(unsigned)rt_vm.opcode,(uint16_t)(next_clock()-start));log(line);
    check(r==VM_WAIT&&rt.ui==RT_UI_MESSAGE,"typed look reaches original game response");if(r!=VM_WAIT)goto stop;
    screen("build/runtime-look.l2");start=next_clock();rt_key(13,0);
    sprintf(line,"DIALOG close=%u frames\n",(uint16_t)(next_clock()-start));log(line);
    r=scan();check(!r&&!rt.ui,"message resumes suspended scan");if(r)goto stop;
    sentence("look gibberish");r=scan();check(r==VM_WAIT&&rt.ui==RT_UI_MESSAGE,"unknown word produces original SQ1 explanation");if(r!=VM_WAIT)goto stop;
    screen("build/runtime-unknown.l2");rt_key(13,0);r=scan();
    sentence("look door");r=scan();check(r==VM_WAIT&&rt.ui==RT_UI_MESSAGE,"second command accepted after closing message");
    if(r==VM_WAIT){rt_key(13,0);r=scan();}
    if(!r)r=performance(0);if(!r)r=performance(7);
    check(!r&&rt_vm.vars[0]==2,"profiled idle and walking scans finish in opening room");
    screen("build/runtime-walking.l2");
    r=input_death_profile();check(!r,"input and settled-death timing comparisons complete");
stop:
    sprintf(line,"game r=%u room=%u logic=%u pc=%u op=%02x test=%u ui=%u\n",(unsigned)r,(unsigned)rt_vm.vars[0],(unsigned)rt_vm.logic,rt_vm.pc,(unsigned)rt_vm.opcode,(unsigned)rt_vm.is_test,(unsigned)rt.ui);log(line);
}
static void dictionary(void){
    uint8_t f,n,r,value,wanted[2],ok=1;uint16_t count=0,group;
    strcpy(path,"games/sq1");r=rt_init(path);if(r){check(0,"dictionary verification initializes");return;}
    strcpy(path,"build/parser-fixtures");resource_cache_init(path);r=agi_vm_start(&rt_vm,0);
    if(r!=VM_RUNNING){check(0,"dictionary pattern fixture opens");return;}
    strcpy(path,"build/parser-words.bin");f=esx_f_open(path,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);
    while(esx_f_read(f,&n,1)==1) {
        if(n>40 || esx_f_read(f,rt.strings[2],n)!=n || esx_f_read(f,wanted,2)!=2){ok=0;break;}
        rt.strings[2][n]=0;group=wanted[0]|((uint16_t)wanted[1]<<8);r=cmd(0x75,2,0);
        value=0;p[0]=1;if(!r && group)r=rt_test(&rt_vm,14,p,3+count*2,&value);
        if(r || rt_vm.vars[9] || (group ? !value : agi_vm_flag(&rt_vm,2))) {
            if(ok){sprintf(line,"dictionary mismatch #%u word=%s group=%u r=%u unknown=%u\n",count,rt.strings[2],group,(unsigned)r,(unsigned)rt_vm.vars[9]);log(line);}ok=0;
        }
        ++count;
    }
    esx_f_close(f);agi_vm_stop(&rt_vm);
    check(ok&&count==738,"all 738 original words match independent synonym IDs");
    strcpy(path,"build/parser-bad-prefix");r=rt_parser_init(path);
    check(!r&&parse("a")==VM_HEADER,"invalid compressed prefix rejected");
    strcpy(path,"build/parser-truncated");r=rt_parser_init(path);
    check(!r&&parse("a")==VM_TRUNCATED,"truncated dictionary entry rejected");
    strcpy(path,"build/parser-bad-header");check(rt_parser_init(path)==VM_HEADER,"invalid dictionary index rejected");
    strcpy(path,"build/parser-no-such-directory");check(rt_parser_init(path)==VM_IO,"missing dictionary fails explicitly");
}
uint8_t bank_parser_tests(void){
    failures=checks=0;strcpy(path,"build/parser-tests.txt");file=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    authored();blocks();dictionary();game();sprintf(line,"SUMMARY checks=%u failures=%u\n",(unsigned)checks,(unsigned)failures);log(line);esx_f_close(file);
    next_text(0,30,failures?"PARSER TESTS FAILED":"PARSER TESTS PASSED");return !failures;
}
