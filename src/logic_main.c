#include <stdio.h>
#include "next/platform.h"
#include "next/logic_resource.h"
static agi_vm vm;
static char text[50];
int main(void) {
    uint8_t r;
    next_init(); next_text(0,3,"ZXQUEST SQ1 LOGIC DIAGNOSTIC");
    resource_cache_init("games/sq1");
    agi_vm_init(&vm,&next_logic_io,next_logic_command,0);
    agi_vm_set_flag(&vm,5,1);
    r=agi_vm_start(&vm,0);
    while(r==VM_RUNNING) r=agi_vm_run(&vm,100);
    sprintf(text,"STATUS %u  LOGIC %u",(unsigned)r,(unsigned)vm.logic); next_text(0,6,text);
    sprintf(text,"OFFSET %u  OPCODE %02X",vm.pc,(unsigned)vm.opcode); next_text(0,8,text);
    sprintf(text,"VAR 10 = %u",(unsigned)vm.vars[10]); next_text(0,10,text);
    next_text(0,13,r==VM_UNSUPPORTED ? "COMMAND NOT IMPLEMENTED YET" : "EXECUTION STOPPED");
    next_text(0,16,"VM DIAGNOSTIC - GAME NOT PLAYABLE");
    for(;;) {}
}
