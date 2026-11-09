#include "next/sound.h"
#include <arch/zxn.h>
extern int bank_runtime_tests_main(void);
extern uint8_t bank_runtime_feedback_tests(void);
uint8_t runtime_feedback_tests(void) {
    uint8_t saved=ZXN_READ_REG(0x54),r;
    ZXN_WRITE_REG(0x54,89); r=bank_runtime_feedback_tests();
    ZXN_WRITE_REG(0x54,saved); return r;
}
extern uint8_t bank_parser_tests(void);
extern uint8_t bank_render_tests(void);
uint8_t parser_tests(void) {
    uint8_t saved=ZXN_READ_REG(0x54),r;
    ZXN_WRITE_REG(0x54,57);r=bank_render_tests();
    ZXN_WRITE_REG(0x54,90);r&=bank_parser_tests();ZXN_WRITE_REG(0x54,saved);return r;
}
int main(void) { next_sound_manual=1; ZXN_WRITE_REG(0x54,88); return bank_runtime_tests_main(); }
