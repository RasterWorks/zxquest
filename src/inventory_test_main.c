#include "next/sound.h"
#include <arch/zxn.h>
#include "next/platform.h"
#include "next/runtime.h"
extern uint8_t bank_inventory_tests(void);
extern uint8_t bank_inventory_game_tests(void);
int main(void) { next_sound_manual=1;
    uint8_t r;next_init();next_clock_start();
    ZXN_WRITE_REG(0x54,57);r=bank_inventory_tests();
    ZXN_WRITE_REG(0x54,89);if(!bank_inventory_game_tests())r=0;
    next_text(0,30,r?"INVENTORY TESTS PASSED":"INVENTORY TESTS FAILED");
    for(;;){}
}
