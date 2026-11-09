#include <arch/zxn.h>
#include "next/platform.h"
#include "next/runtime.h"
#include "next/sound.h"
extern uint8_t bank_sound_tests(void);
extern uint8_t bank_sound_game_tests(void);
int main(void) {
    uint8_t ok;next_init();next_sound_manual=1;next_clock_start();
    ZXN_WRITE_REG(0x54,57);ok=bank_sound_tests();
    ZXN_WRITE_REG(0x54,89);if(!bank_sound_game_tests())ok=0;next_sound_halt();
    next_text(0,30,ok?"SOUND TESTS PASSED":"SOUND TESTS FAILED");for(;;){}
}
