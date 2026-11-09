# LOGIC VM foundation

The VM executes AGI v2 bytecode directly from the separate game files through
the banked resource cache. The core diagnostic builds remain available, and
the default sprite viewer is still separate. The combined `-Runtime` build
now provides startup/game-cycle callbacks; see [runtime.md](runtime.md).

## Build and run

```powershell
python tools/prepare_logic_tests.py
.\build.ps1 -LogicTest
.\run.ps1 -LogicTest
```

Completion is the `SUMMARY` line in `build/logic-tests.txt`, with zero failures.
The host tool also writes `build/logic-inventory.json`, listing commands used
by every supplied LOGIC. Generated files contain local test data and stay
ignored. No original script or message bytes are compiled into either NEX.

To run just the original SQ1 startup until its first unsupported instruction:

```powershell
.\build.ps1 -Logic
.\run.ps1 -Logic
```

The diagnostic sets flag 5 (first room scan) and starts LOGIC 0 with otherwise
zeroed VM state. This is a deliberately limited startup harness, not complete
AGI initialization. Expected first stop: LOGIC 104, resource offset 8, command
`$96` (`trace.info`). LOGIC 0 assigns variable 10 to 2 before calling it.
Direct room-1 first-scan execution reaches `set.horizon` at resource offset 8.
These stops are explicit unsupported results, not successful game execution.

## Implemented instructions

* Commands `$00–$17`: return, saturating increment/decrement, assignments,
  wrapping addition/subtraction, three forms of indirection, direct/variable
  set/reset/toggle flags, room events, LOGIC preloads and calls.
* Tests 1–8: equality, less-than, greater-than (immediate and variable), and
  direct/variable flag tests. AND, OR and NOT preserve AGI short-circuiting.
* `$fe` signed relative jumps and `$ff` conditional blocks. IF false-branch
  lengths are unsigned. All byte reads and taken branches are code-bounded.
* `$91/$92`: set/reset scan start. `$a5–$a8`: multiplication/division.
  Division by zero produces a diagnostic rather than undefined C behaviour.
* The Next callback implements picture, VIEW and SOUND preloads (`$18`,
  `$1e`, `$1f`, `$62`) through the shared cache. Executing LOGICs are pinned;
  preloads are released and may be evicted without changing game data.

Other commands and active tests need host callbacks. The core knows the
operand lengths of 183 commands and 18 tests, including the variable-length
`said` word list. Consuming or skipping those operands is not implementation
of the associated gameplay behaviour. Parser, controller, object and string
tests need runtime callbacks. The combined runtime implements controller,
key, actor, inventory, `said` and string-comparison tests. Parser matching
reads the separate WORDS.TOK dictionary; the core VM remains independent
of vocabulary and input handling.

The command-arity table targets the later AGI v2 layout (2.440 onward). The
host structurally decodes all 101 supplied LOGICs and verifies every branch
lands on an instruction boundary with this layout. Older interpreter arity
variants, AGI v3 decompression, and game-specific compatibility adjustments
remain future work. Runtime jumps validate code range, not a precomputed
instruction-boundary bitmap.

## API and memory

`src/agi/logic.h` separates the VM from the Next. The I/O adapter supplies
open, close and copy operations with opaque 32-bit handles. Each of the 16
call frames holds a pinned resource, PC, code end and resource length.
Calls use this explicit array, not recursive C calls on the 1 KB CPU stack.
A shared 64-byte resident fetch window reduces bank-copy overhead. Its cursor
belongs to the current call frame: calls/returns invalidate it, while branches
and `said` word-list skips explicitly seek within it or invalidate it. Refills
use the frame's complete resource handle and never extend past its code end.
Sequential opcode/operand reads then use the cursor without repeating handle
comparisons and frame lookup. Yield/resume preserves the cursor; stopping or
starting execution clears it. The dispatcher refreshes its local frame pointer
only on calls/returns, retaining token budgets and exact error locations.

The VM keeps 256 byte variables, 256 packed flags and 256 16-bit scan starts.
The complete VM instance occupies 1,067 resident bytes with z88dk. The cursor
replaces a redundant 32-bit window identity, saving three resident bytes.
See [validation.md](validation.md) for current combined-runtime memory limits.
`agi_vm_init` initializes fresh storage. Call `agi_vm_stop` before reinitializing
an active instance. `agi_vm_start` cancels any suspended execution but retains
variables, flags and scan starts. `agi_vm_reset_scans` belongs to room lifecycle
handling. Callbacks cannot re-enter, start or stop their VM.

`agi_vm_run(vm, budget)` charges one unit per command or condition token and
returns `VM_RUNNING` when the budget is exhausted. Conditions retain their
state across yields, including OR short-circuiting and NOT. This bounds loops
and allows the future scheduler to service input, timers and animation. It
does not impose a wall-clock limit on filesystem access or host callbacks.

Command callbacks may now return `VM_WAIT` for modal host input. Frames and
pins stay live; subsequent `run` calls do not advance until the host calls
`agi_vm_resume`. The next instruction is already recorded, so print/get.string
are not executed twice. Test callbacks cannot suspend.

Return, cancellation, room events and errors release all execution pins.
The failing logic number, resource-relative PC, opcode and command/test mode
survive unwinding. Missing resources, invalid headers, truncated operands,
out-of-range branches and call-depth overflow have distinct status codes.
`VM_ROOM` returns the requested room number to the host; resetting actors,
flags, loaded state and restarting LOGIC 0 are still the host's responsibility.

The message reader validates the count/offset table and declared text end,
then decrypts copied bytes with the AGI v2 `Avis Durgan` key. It never alters
cached raw bytes. `VM_MORE` supports small, NUL-terminated chunks of long
messages; offsets supplied by callers should be the number of bytes already
read from that message. Buffers require at least two bytes so every `VM_MORE`
result makes progress while preserving a NUL terminator.
The combined runtime adds basic `%s`/`%v` formatting and modal text windows;
these remain separate from the raw message API. Zero offsets and encrypted
empty strings both work.

## Provenance

The command arities and core arithmetic/flag semantics are adapted from
Sarien's `src/op_dbg.c`, `src/op_cmd.c` and `src/op_test.c`, pinned at
`aaef5e667757385b5b3650cc1f7365908720f7e6`. Copyright and GPL notices are retained
in `src/agi/logic.c` and `src/agi/opcodes.c`; the upstream GNU GPL version 2
text is included as root `COPYING`. The bounded reader, explicit stack,
incremental condition evaluator and immutable message API replace upstream
flat-pointer assumptions. No original Sierra executable code is used.

References:

* [Pinned Sarien source](https://github.com/cmatsuoka/sarien/tree/aaef5e667757385b5b3650cc1f7365908720f7e6)
* [AGI LOGIC format and interpreter disassembly, Lance Ewing et al.](https://www.agidev.com/articles/agispec/agispecs-6.html)

## Next integration

The combined runtime supplies game-cycle state, actors, text and menus in
separate 8 KB overlays. `-BankedLogicTest` runs the same core regression suite
with the VM mapped in page 50. `-RuntimeTest` checks callbacks, modal
continuations and original SQ1 startup; see [runtime.md](runtime.md).
