#ifndef NEXT_SAVE_H
#define NEXT_SAVE_H
#include <stdint.h>
/* ZXQuest format, not Sierra DOS saves. Call outside agi_vm_run only.
   Restore stages and validates the complete snapshot before changing state. */
enum { SAVE_OK, SAVE_IO, SAVE_INVALID, SAVE_GAME, SAVE_MEMORY, SAVE_EMPTY };
extern uint8_t rt_save_identity_valid;
uint8_t rt_save_slot(uint8_t slot,uint8_t restore);
uint8_t rt_save_ui(uint8_t op,uint8_t ascii,uint8_t scan);
void rt_parser_reset_input(void);
uint8_t rt_startup(void);
#endif
