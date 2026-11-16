#include "next/sound.h"
#include <string.h>
#include <arch/zxn.h>
#include "next/platform.h"
#include "next/runtime.h"
extern uint8_t bank_parser_tests(void);
extern uint8_t bank_render_tests(void);
/* Resident reader: the renderer maps its own code bank during callbacks. */
uint8_t render_test_read(void *context,uint16_t at,void *data,uint16_t n){memcpy(data,(uint8_t *)context+at,n);return 0;}
/* Distinct callback selects the uncached reference path for priority tests. */
extern uint8_t next_view_read(void *,uint16_t,void *,uint16_t);
uint8_t render_resource_read(void *context,uint16_t at,void *data,uint16_t n){return next_view_read(context,at,data,n);}
int main(void){uint8_t r;next_init();next_clock_start();ZXN_WRITE_REG(0x54,57);r=bank_render_tests();ZXN_WRITE_REG(0x54,90);r&=bank_parser_tests();if(!r)next_text(0,30,"PARSER/RENDER TEST FAILED");for(;;){}}
