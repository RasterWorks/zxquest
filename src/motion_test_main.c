#include "next/sound.h"
#include <arch/zxn.h>
#include "next/platform.h"
#include "next/runtime.h"
extern uint8_t bank_motion_tests(void);
extern uint8_t bank_room_tests(void);
extern uint8_t bank_follow_tests(void);
int main(void) { next_sound_manual=1;
    uint8_t r;next_init();next_clock_start();
    ZXN_WRITE_REG(0x54,90);r=bank_motion_tests();
    ZXN_WRITE_REG(0x54,57);if(!bank_follow_tests())r=0;
    ZXN_WRITE_REG(0x54,94);r&=bank_room_tests();
    next_text(0,30,r?"MOTION/ROOM TESTS PASSED":"MOTION/ROOM TESTS FAILED");
    for(;;){}
}
