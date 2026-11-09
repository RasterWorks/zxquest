#ifndef AGI_LOGIC_H
#define AGI_LOGIC_H
#include <stdint.h>

#define AGI_VM_DEPTH 16
#define AGI_VM_WINDOW 64
enum { VM_DONE, VM_RUNNING, VM_ROOM, VM_IO, VM_HEADER, VM_TRUNCATED,
       VM_OPCODE, VM_TEST, VM_BRANCH, VM_STACK, VM_UNSUPPORTED,
       VM_ARGUMENT, VM_MESSAGE, VM_MORE, VM_WAIT };

typedef struct {
    uint8_t (*open)(uint8_t number, uint32_t *handle, uint16_t *size);
    void (*close)(uint32_t handle);
    uint8_t (*read)(uint32_t handle, uint16_t offset, void *data, uint16_t count);
} agi_logic_io;
typedef struct {
    uint32_t handle;
    uint16_t pc, end, size;
    uint8_t number;
} agi_logic_frame;
typedef struct agi_vm agi_vm;
/* Command callbacks return VM_DONE, VM_WAIT for a host modal interaction, or
   a VM error. Test callbacks return VM_DONE or an error. Arguments are raw AGI
   operands. A said test supplies count in p[0], and its word-list offset in at.
   Callbacks must not re-enter/start/stop the VM; use read_code for said words. */
typedef uint8_t (*agi_command_fn)(agi_vm *, uint8_t op, const uint8_t *p);
typedef uint8_t (*agi_test_fn)(agi_vm *, uint8_t op, const uint8_t *p,
                             uint16_t at, uint8_t *value);
struct agi_vm {
    agi_logic_io io;
    agi_command_fn command;
    agi_test_fn test;
    uint8_t vars[256], flags[32];
    uint16_t scan[256];
    agi_logic_frame frames[AGI_VM_DEPTH];
    /* Fetch window belongs to the top frame. Calls/returns invalidate it;
       branches explicitly seek it. Sequential reads need only this cursor. */
    uint8_t window_pos;
    uint16_t window_at;
    uint8_t window_size, window[AGI_VM_WINDOW];
    uint8_t depth, status, room;
    uint8_t logic, opcode, is_test; /* diagnostic location survives unwinding */
    uint16_t pc;
    uint8_t in_if, and_value, in_or, or_value, negate;
};

/* Init once on fresh storage; stop before reinitializing an active VM. */
void agi_vm_init(agi_vm *vm, const agi_logic_io *io,
                 agi_command_fn command, agi_test_fn test);
uint8_t agi_vm_start(agi_vm *vm, uint8_t logic);
/* Each command or condition token costs one unit. RUNNING means yield/resume.
   Terminal return, room event and errors release all executing resources.
   Scan starts persist between starts; reset_scans belongs to room lifecycle. */
uint8_t agi_vm_run(agi_vm *vm, uint16_t budget);
/* Resume after the host has completed the pending modal interaction. */
uint8_t agi_vm_resume(agi_vm *vm);
void agi_vm_stop(agi_vm *vm);
void agi_vm_reset_scans(agi_vm *vm);
uint8_t agi_vm_flag(const agi_vm *vm, uint8_t flag);
void agi_vm_set_flag(agi_vm *vm, uint8_t flag, uint8_t value);
uint8_t agi_vm_read_code(agi_vm *vm, uint16_t at, void *data, uint16_t count);
uint8_t agi_logic_header(const agi_logic_io *io, uint32_t handle,
                         uint16_t size, uint16_t *end);
/* AGI v2 encrypted text. offset is a plaintext byte offset. Capacity must be
   at least 2, allowing progress plus NUL. For valid arguments buffer is always
   terminated; written excludes NUL. MORE permits incremental reads of long
   messages. Zero message offsets denote empty strings. Raw data never changes. */
uint8_t agi_logic_message(const agi_logic_io *io, uint32_t handle, uint16_t size,
                          uint8_t number, uint16_t offset, char *text,
                          uint16_t capacity, uint16_t *written);
extern const uint8_t agi_command_args[183]; /* AGI 2.440+ arities */
#endif
