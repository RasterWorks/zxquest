#include "next/sound.h"
#include <arch/zxn.h>
#include "next/platform.h"
#include "next/runtime.h"
extern uint8_t bank_parser_tests(void);
extern uint8_t bank_render_tests(void);
int main(void){uint8_t r;next_init();next_clock_start();ZXN_WRITE_REG(0x54,57);r=bank_render_tests();ZXN_WRITE_REG(0x54,90);r&=bank_parser_tests();if(!r)next_text(0,30,"PARSER/RENDER TEST FAILED");for(;;){}}
