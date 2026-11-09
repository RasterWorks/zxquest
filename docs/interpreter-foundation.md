# Interpreter foundation — 23 October 2026

## Decision

Use **Sarien** as the source foundation for staged AGI C engine adaptation.
Keep the working Next graphics/filesystem layers, replace flat resource
pointers with handles and offsets, and import tested engine components in
small steps. The first VM adaptation now runs in the separate LOGIC diagnostic
and test builds; the default sprite viewer is still separate. See [logic-vm.md](logic-vm.md).

Evaluated upstream: <https://github.com/cmatsuoka/sarien>, revision
`aaef5e667757385b5b3650cc1f7365908720f7e6`. The checkout is under
`build/upstream/sarien`, outside tracked project sources. The license file
`doc/COPYING` contains GNU GPL version 2, and the evaluated source files carry
the Sarien authors' copyright and GPL notices. Preserve those notices and
include the license and adapted source when importing/distributing the engine.
Adapted opcode semantics/tables now reside in `src/agi/logic.c` and
`src/agi/opcodes.c`, retaining notices, with the upstream license in root
`COPYING`. Sierra's original game files remain separate and ignored by Git.

Sarien has a portable C VM, tests, motion and resource modules with clear
source provenance. CX16Agi is a useful banked-machine reference, but its
6502 assembly and Commander X16 display integration need substantial
replacement on Z80N. ScummVM remains a compatibility reference, with a much
larger C++ platform layer than this project needs for its initial target.

## Reproducible z88dk probe

```powershell
git clone https://github.com/cmatsuoka/sarien.git build/upstream/sarien
git -C build/upstream/sarien checkout aaef5e667757385b5b3650cc1f7365908720f7e6
.\tools\probe_sarien.ps1
```

The script checks the pinned revision, copies just the needed C files and
headers to `build/sarien-probe`, corrects `UINT32`/`SINT32` to `long` (z88dk
`int` is 16 bits), disables optional host features and supplies a version
string. It compiles with `+zxn -clib=sdcc_iy -O2` and assembles objects. This
is a compiler/size probe; unresolved external engine/platform calls are not
linked or executed, and no compatibility claim follows from compilation.

Measured object sections, before linking supporting library functions:

| Source | Code bytes | Constant bytes | Writable data/BSS bytes |
| --- | ---: | ---: | ---: |
| `logic.c` | 691 | 1 | 0 |
| `op_cmd.c` | 12,764 | 894 | 370 |
| `op_test.c` | 3,637 | 37 | 0 |
| `motion.c` | 1,868 | 0 | 0 |

The compiler reports `sizeof(struct agi_game) = 37,010`, directory entry
14 bytes and actor entry 43 bytes. The game structure alone exceeds the
31,488 bytes between current resident origin `$8000` and the reserved stack
at `$fb00`. It also retains pointers to separately allocated data. A direct
rebuild with only a graphics driver replacement therefore cannot fit this
layout. Compilation has warnings, including old-style prototypes, unused
arguments, const qualifiers, and a one-past-array expression in the motion
iteration macro. Review the 255-entry actor table against AGI object 255
semantics during adaptation; do not suppress these warnings indiscriminately.

## Porting constraints and budget

* Keep only active resource descriptors in resident RAM. The cache has 32
  handle entries. It originally used pages 64–95 (256 KB); the parser milestone
  reserves pages 88–95 for runtime/test growth and uses pages 64–87 (192 KB).
  This remains within base 1 MB hardware. No flat resource pointer survives
  a bank switch. See [runtime.md](runtime.md) for the current allocation.
* The VM now fetches through a 64-byte resident instruction window, with a
  pinned handle per active call frame. LOGIC message lookup uses offsets
  and decrypts copied text without mutating cached raw resources.
* Actor state requires separate banked storage or a compact active-object
  representation. Preserve all 256 possible object IDs. Budget up to 16 KB
  for a straightforward banked actor table before attempting compression.
* Picture/composite pages 32–35, clean-background pages 36–39 and display
  pages 40–49 leave 14 pages among 32–63 unused by production code (112 KB).
  Budget these initially for 16 KB actor state, 16 KB other engine state/work
  areas and up to 80 KB banked code. The VIEW milestone consumes the extra
  32 KB background snapshot. This is a provisional capacity budget, not
  linked sections; a smaller sprite save-area scheme could recover that RAM.
* A candidate code layout moves resident code to `$a000` and uses MMU4 at
  `$8000` for 8 KB code overlays, retaining MMU2 for pixels and MMU3 for
  resources/display. That leaves 23,296 resident bytes before the stack.
  Prototype and measure the call gate before importing large opcode units;
  `op_cmd.c` must be split across banks or reduced. Current builds still
  use the existing `$8000` resident origin.
  The sprite viewer now occupies 23,810 resident bytes, 514 bytes more than
  that proposed `$a000` capacity. Move its temporary 4 KB picture input buffer
  or bank some renderer code before changing the resident origin.
* Add bounded opcode decoding, resource/call errors and execution budgeting
  when porting the VM. Do not silently treat missing gameplay commands as
  implemented. Compatibility testing must include actual room logic, parser,
  movement and a second AGI game later.

## Resource cache contract

`resource_cache_init(directory)` resets the cache for one game and invalidates
all handles, even pinned ones. Initialize only when abandoning the previous
game. Invalid initialization leaves the existing cache intact.

`resource_acquire(type, number, &handle)` returns a pinned, immutable resource.
Every successful acquire, including a cache hit, needs one `resource_release`.
Released entries remain cached until least-recently-used eviction. Pin counts
cannot wrap. Handles are monotonically assigned 32-bit tokens; stale handles
fail rather than referring to reused slots. Token exhaustion reports failure
rather than wrapping. Diagnostic hit/miss/eviction counters are 16-bit and
may wrap independently of the LRU order.

`resource_size` and `resource_copy` operate on handles. Copy validates offset
and length without overflowing 16-bit arithmetic and splits across 8 KB
banks. Resources may use noncontiguous physical pages. A 512-byte resident
staging buffer separates filesystem calls from memory mapping. Cache calls
preserve MMU2 and MMU3; caller buffers must reside above `$8000` and must not
overlap internal cache state. This synchronous API is not ISR/reentrant safe;
future interrupt handlers must not change the shared bank window.

In the combined banked runtime, buffers passed between overlays must instead
remain at or above `$a000` (resident globals or stack), because `$8000–$9fff`
is the code window. The new resident timer does not access resources or
change MMU mappings. See [runtime.md](runtime.md) for the active layout.

The cache preflights capacity before evicting anything. Pinned resources are
never victims. If disk reading fails after eviction, unused old entries may
have been evicted, but the partial new entry is discarded and all its pages
are freed. Failure returns handle zero. Raw payloads can be up to 65,535
bytes; zero-length payloads are supported. AGI v3 decompression is not yet
implemented. Physical Next OS bank reservation remains a separate hardware
integration task.
