#ifndef NEXT_LOGIC_RESOURCE_H
#define NEXT_LOGIC_RESOURCE_H
#include "agi/logic.h"
#include "resource_cache.h"
extern const agi_logic_io next_logic_io;
/* Resource preloads only. Other commands fail explicitly until a game runtime
   supplies their implementation. The raw cache may evict released preloads. */
uint8_t next_logic_command(agi_vm *vm, uint8_t op, const uint8_t *p);
#endif
