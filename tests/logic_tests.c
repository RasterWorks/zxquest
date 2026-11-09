#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "next/platform.h"
#include "next/logic_resource.h"
#include "../build/logic_manifest.h"

static agi_vm vm;
static resource_cache_info info;
static uint16_t checks,failures,logics,messages,callback_count,read_count;
static uint32_t text_bytes;
static uint8_t log_file,fail_reads;
static char line[100],text[65];
static const uint16_t crc_table[16]={
    0,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
    0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef
};
static uint16_t crc_byte(uint16_t crc,uint8_t b) {
    crc=(crc<<4)^crc_table[(crc>>12)^(b>>4)];
    return (crc<<4)^crc_table[(crc>>12)^(b&15)];
}
static void log_line(const char *s) {
    uint16_t n=strlen(s);
    if(esx_f_write(log_file,(void *)s,n)!=n) {
        next_text(0,28,"LOG WRITE FAILED"); for(;;) {}
    }
}
static void check(uint8_t ok,const char *name) {
    ++checks; if(!ok) ++failures;
    sprintf(line,"%s %s\n",ok ? "PASS" : "FAIL",name); log_line(line);
}
static uint8_t read_fault(uint32_t h,uint16_t at,void *data,uint16_t n) {
    ++read_count;
    if(fail_reads && at>=8000) return 1;
    return resource_copy(h,at,data,n);
}
static uint8_t said_test(agi_vm *v,uint8_t op,const uint8_t *p,uint16_t at,uint8_t *value) {
    uint8_t b[4],r;
    ++callback_count;
    if(op!=14 || p[0]!=2) return VM_UNSUPPORTED;
    r=agi_vm_read_code(v,at,b,4); if(r) return r;
    *value=b[0]==255 && b[1]==255 && b[2]==252 && b[3]==253;
    return VM_DONE;
}
static uint8_t pressure(agi_vm *v,uint8_t op,const uint8_t *p) {
    uint8_t i; resource_handle h;
    (void)v; (void)p;
    if(op!=0x90) return VM_UNSUPPORTED;
    for(i=0;i<40;++i) {
        if(resource_acquire(RESOURCE_LOGIC,i,&h)) return VM_IO;
        resource_release(h);
    }
    return VM_DONE;
}
static void fresh(void) {
    agi_vm_stop(&vm);
    agi_vm_init(&vm,&next_logic_io,0,0);
    callback_count=read_count=fail_reads=0;
}
static uint8_t run(uint8_t n,uint16_t budget) {
    uint8_t r; uint16_t turns=0;
    r=agi_vm_start(&vm,n);
    while(r==VM_RUNNING && turns++<1000) r=agi_vm_run(&vm,budget);
    return r;
}
static uint8_t released(void) {
    resource_cache_status(&info); return !info.pinned && !vm.depth;
}
static void expect_error(uint8_t n,uint8_t result,const char *name) {
    fresh();
    check(run(n,17)==result && released(),name);
}
static void fixture_tests(void) {
    uint8_t r,mmu2,mmu3; uint16_t n,size,got,end; resource_handle h;
    resource_cache_init("build/logic-fixtures");
    fresh();
    r=run(0,1);
    check(r==VM_DONE && vm.vars[0]==1 && vm.vars[1]==255 && vm.vars[4]==9 &&
          vm.vars[5]==9 && vm.vars[20]==9 && vm.vars[255]==255 &&
          agi_vm_flag(&vm,255) && !agi_vm_flag(&vm,254) && released(),
          "saturating increment/decrement, wrapping arithmetic, indirection and flags");
    fresh(); vm.test=said_test;
    check(run(1,1)==VM_DONE && vm.vars[2]==42 && !callback_count,
          "resumable AND OR NOT and short-circuit side effects");
    fresh(); check(!run(40,1) && vm.vars[5]==42,"all six comparisons and direct/variable flag tests");
    fresh(); check(!run(41,1) && !vm.vars[1],"all-false OR skips guarded commands");
    fresh(); check(run(2,1)==VM_DONE && vm.vars[42]==2 && vm.vars[43]==1 && released(),
          "nested immediate/variable calls, preloads and return to caller");
    fresh(); r=run(5,1); r|=run(5,1);
    check(!r && vm.vars[1]==1 && vm.vars[2]==2 && vm.scan[5]==5,
          "scan start is the following instruction and persists across scans");
    agi_vm_reset_scans(&vm);
    check(!run(5,1) && vm.vars[1]==2 && vm.vars[2]==3,"room lifecycle can reset scan starts");
    fresh(); r=run(6,1); r|=run(6,1);
    check(!r && vm.vars[1]==2 && vm.vars[2]==2,"reset.scan.start executes from beginning again");
    fresh(); check(!run(7,1) && vm.vars[0]==10,"signed backward branch and conditional exit");
    fresh(); r=agi_vm_start(&vm,8); r=agi_vm_run(&vm,100);
    check(r==VM_RUNNING && vm.depth==1 && vm.frames[0].pc==2,"self-loop yields at instruction budget");
    r=agi_vm_run(&vm,0);
    check(r==VM_RUNNING && vm.frames[0].pc==2,"zero budget changes no script state");
    agi_vm_stop(&vm); check(released(),"cancel releases suspended script");
    expect_error(9,VM_STACK,"recursive call depth limit releases all 16 pins");
    fresh(); r=run(10,1);
    check(r==VM_ROOM && vm.room==123 && !vm.vars[1] && released(),"new.room.v unwinds every caller");
    fresh(); check(run(30,1)==VM_ROOM && vm.room==77 && released(),"immediate new.room event");
    expect_error(12,VM_TRUNCATED,"truncated command cannot consume message bytes");
    expect_error(13,VM_OPCODE,"unknown command rejected before table lookup");
    expect_error(14,VM_BRANCH,"negative branch cannot wrap into header");
    expect_error(15,VM_TRUNCATED,"truncated test operands rejected");
    expect_error(16,VM_TEST,"unknown test rejected");
    expect_error(17,VM_TEST,"unclosed OR rejected");
    expect_error(18,VM_HEADER,"oversized code section rejected");
    expect_error(19,VM_IO,"missing callee releases caller pin");
    expect_error(20,VM_BRANCH,"positive branch cannot enter message area");
    fresh(); vm.test=said_test;
    check(!run(21,1) && !callback_count && !vm.vars[1],"false AND skips said words containing delimiter bytes");
    fresh(); vm.test=said_test;
    check(!run(22,1) && callback_count==1 && vm.vars[1]==42,"active said exposes bounded word-list offset");
    fresh(); vm.io.read=read_fault; mmu2=ZXN_READ_REG(0x52); mmu3=ZXN_READ_REG(0x53);
    r=run(23,1);
    check(!r && vm.vars[255]==42 && vm.vars[42]==1 && read_count<12 &&
          ZXN_READ_REG(0x52)==mmu2 && ZXN_READ_REG(0x53)==mmu3 && released(),
          "opcode operands cross 8KB banks, buffered fetch and caller restore preserve MMUs");
    fresh(); r=run(24,1);
    check(r==VM_UNSUPPORTED && vm.logic==24 && vm.pc==2 && vm.opcode==0x65 && !vm.is_test && released(),
          "unsupported command retains exact diagnostic location");
    fresh(); r=run(25,1);
    check(r==VM_UNSUPPORTED && vm.logic==25 && vm.pc==3 && vm.opcode==13 && vm.is_test && released(),
          "unsupported test retains exact diagnostic location");
    fresh(); check(!run(26,1) && vm.vars[0]==13,"multiply/divide immediate and variable");
    fresh(); check(!run(42,1) && vm.vars[0]==1 && vm.vars[1]==1,"16-bit target multiplication wraps 255 squared safely");
    expect_error(27,VM_ARGUMENT,"division by zero rejected");
    expect_error(28,VM_TEST,"dangling NOT rejected");
    expect_error(29,VM_TRUNCATED,"oversized said list rejected before callback");
    fresh(); vm.command=pressure;
    check(!run(32,1) && vm.vars[42]==1 && released(),"cache pressure cannot evict executing parent or child");
    resource_cache_status(&info); check(info.evictions>0,"cache pressure actually caused eviction");
    expect_error(33,VM_TRUNCATED,"unterminated condition reports code boundary");
    fresh(); vm.command=next_logic_command;
    check(run(35,1)==VM_IO && released(),"failed resource command unwinds VM");
    expect_error(39,VM_BRANCH,"false IF validates unsigned skip range");
    fresh(); check(!run(38,1),"true IF ignores unused skip distance");
    fresh();check(!run(46,1)&&vm.vars[255]==43,"operand window refill across yields");
    fresh();r=run(47,1);n=!r&&!vm.vars[71];fresh();agi_vm_set_flag(&vm,3,1);
    check(n&&!run(47,1)&&vm.vars[71]==99,"true/false IF offset crosses fetch window");
    fresh();vm.test=said_test;
    check(!run(48,1)&&callback_count==1&&vm.vars[72]==42,"said skip crosses window with delimiter bytes");
    fresh(); vm.io.read=read_fault; fail_reads=1;
    check(run(23,1)==VM_IO && released(),"instruction-window read failure releases resource");
    fresh(); agi_vm_start(&vm,8);
    check(agi_vm_start(&vm,3)==VM_RUNNING && !agi_vm_run(&vm,10) && released(),"starting new script cancels suspended stack");
    resource_acquire(RESOURCE_LOGIC,31,&h); resource_size(h,&size);
    agi_logic_header(&next_logic_io,h,size,&end);
    r=agi_logic_message(&next_logic_io,h,size,2,0,text,sizeof(text),&got);
    check(!r && got==18 && !strcmp(text,"Cross-bank message"),"message offset/decryption crosses a bank boundary");
    r=agi_logic_message(&next_logic_io,h,size,1,8170,text,sizeof(text),&got);
    check(!r && got==11 && !strcmp(text,"AAAAAAAAAAA"),"incremental message resumes with correct XOR phase");
    r=agi_logic_message(&next_logic_io,h,size,1,0,text,4,&got);
    check(r==VM_MORE && got==3 && !strcmp(text,"AAA"),"long message chunks stay NUL terminated");
    r=agi_logic_message(&next_logic_io,h,size,3,0,text,sizeof(text),&got);
    check(!r && !got && !text[0],"zero offset message is empty");
    r=agi_logic_message(&next_logic_io,h,size,4,0,text,sizeof(text),&got);
    check(!r && !got,"encrypted empty string is empty");
    check(agi_logic_message(&next_logic_io,h,size,0,0,text,sizeof(text),&got)==VM_MESSAGE &&
          agi_logic_message(&next_logic_io,h,size,5,0,text,sizeof(text),&got)==VM_MESSAGE,
          "message number bounds");
    check(agi_logic_message(&next_logic_io,h,size,1,65535,text,sizeof(text),&got)==VM_MESSAGE,
          "message offset cannot wrap");
    check(agi_logic_message(&next_logic_io,h,size,1,0,text,0,&got)==VM_ARGUMENT &&
          agi_logic_message(&next_logic_io,h,size,1,0,text,1,&got)==VM_ARGUMENT,
          "message buffer must allow progress and NUL terminator");
    resource_release(h);
    for(n=36;n<=37;++n) {
        resource_acquire(RESOURCE_LOGIC,n,&h); resource_size(h,&size);
        r=agi_logic_message(&next_logic_io,h,size,1,0,text,sizeof(text),&got);
        check(r==VM_MESSAGE,n==36 ? "message end beyond resource rejected" : "unterminated message rejected");
        resource_release(h);
    }
    for(n=43;n<=44;++n) {
        resource_acquire(RESOURCE_LOGIC,n,&h); resource_size(h,&size);
        r=agi_logic_message(&next_logic_io,h,size,1,0,text,sizeof(text),&got);
        check(r==VM_MESSAGE,n==43 ? "message pointer into offset table rejected" : "truncated message table rejected");
        resource_release(h);
    }
}
static void game_tests(void) {
    uint16_t i,m,size,got,offset,crc,bytes,j; uint8_t r,ok;
    resource_handle h;
    const struct logic_expected *e;
    fresh(); resource_cache_init("games/sq1");
    for(i=0;i<sizeof(expected)/sizeof(expected[0]);++i) {
        e=&expected[i]; ok=1; crc=0xffff; bytes=0;
        if(resource_acquire(RESOURCE_LOGIC,e->number,&h)) { ++failures; continue; }
        resource_size(h,&size);
        for(m=1;m<=e->messages;++m) {
            offset=0;
            do {
                r=agi_logic_message(&next_logic_io,h,size,(uint8_t)m,offset,text,sizeof(text),&got);
                if(r!=VM_DONE && r!=VM_MORE) { ok=0; break; }
                for(j=0;j<got;++j) crc=crc_byte(crc,text[j]);
                offset+=got; bytes+=got;
            } while(r==VM_MORE);
            if(!ok) break;
            crc=crc_byte(crc,0); ++bytes; ++messages;
        }
        resource_release(h);
        if(!ok || bytes!=e->bytes || crc!=e->crc) {
            ++failures; sprintf(line,"FAIL LOGIC %u messages crc=%04x expected=%04x\n",(unsigned)e->number,crc,e->crc); log_line(line);
        } else { ++logics; text_bytes+=bytes; }
        if(!(i&15)) { sprintf(line,"LOGIC TEXT CHECKED %u             ",i+1); next_text(0,5,line); }
    }
    fresh(); vm.command=next_logic_command;
    /* Execute an authored load sequence against unmodified game resources. */
    vm.vars[0]=1;
    { static const uint8_t p[]={0}; static const uint8_t one[]={1};
      check(!next_logic_command(&vm,0x18,p) && !next_logic_command(&vm,0x1e,one) &&
            !next_logic_command(&vm,0x1f,p) && !next_logic_command(&vm,0x62,one) && released(),
            "actual SQ1 picture, view and sound preloads through VM adapter"); }
    fresh(); vm.command=next_logic_command;
    agi_vm_set_flag(&vm,5,1); /* first room scan */
    r=run(0,1);
    sprintf(line,"SQ1 startup status=%u logic=%u pc=%u opcode=%02x test=%u v10=%u\n",
            (unsigned)r,(unsigned)vm.logic,vm.pc,(unsigned)vm.opcode,(unsigned)vm.is_test,(unsigned)vm.vars[10]); log_line(line);
    check(r==VM_UNSUPPORTED && vm.logic==104 && vm.pc==8 && vm.opcode==0x96 &&
          vm.vars[10]==2 && released(),"unmodified SQ1 logic 0 calls 104, stops explicitly at trace.info");
    fresh(); vm.vars[0]=1; agi_vm_set_flag(&vm,5,1);
    r=run(1,1);
    check(r==VM_UNSUPPORTED && vm.logic==1 && vm.pc==8 && vm.opcode==0x3f && released(),
          "unmodified room 1 follows first-scan condition to set.horizon");
}
int main(void) {
    uint8_t i,guard=1;
    memset((void *)0xfb00,0xa5,16);
    next_init(); next_text(0,3,"ZXQUEST LOGIC VM TESTS");
    errno=0; log_file=esx_f_open("build/logic-tests.txt",ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    if(errno) { next_text(0,5,"CANNOT OPEN LOGIC TEST LOG"); for(;;) {} }
    fixture_tests(); game_tests();
    for(i=0;i<16;++i) if(((uint8_t *)0xfb00)[i]!=0xa5) guard=0;
    check(guard,"CPU stack boundary canary intact after deep calls and message reads");
    sprintf(line,"SUMMARY logics=%u messages=%u bytes=%lu checks=%u failures=%u\n",
            logics,messages,text_bytes,checks,failures); log_line(line); esx_f_close(log_file);
    sprintf(line,"LOGICS %u  MESSAGES %u              ",logics,messages); next_text(0,5,line);
    sprintf(line,"CHECKS %u  FAILURES %u             ",checks,failures); next_text(0,7,line);
    next_text(0,9,failures ? "LOGIC TESTS FAILED" : "LOGIC TESTS PASSED");
    next_text(0,12,"SQ1: LOGIC 0 CALLS LOGIC 104");
    next_text(0,14,"STOPS AT TRACE.INFO, OFFSET 8");
    next_text(0,17,"GAME COMMANDS ARE THE NEXT STAGE");
    for(;;) {}
}
