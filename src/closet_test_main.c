#include <arch/zxn.h>
#include "next/platform.h"
extern uint8_t bank_motion_tests(void);
extern int bank_ui_tests_main(void);
int main(void){next_init();ZXN_WRITE_REG(0x54,90);bank_motion_tests();ZXN_WRITE_REG(0x54,88);return bank_ui_tests_main();}
