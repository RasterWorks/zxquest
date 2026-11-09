# Architecture and implementation status

## Separation

* `src/agi/picture.c`: platform-independent picture commands, integer line
  rasterization and bounded nonrecursive scanline fills.
* `src/agi/view.c`: bounded VIEW header/cel reader, streaming RLE decode,
  mirroring and priority-aware sprite drawing through the pixel callbacks.
* `src/agi/logic.c` and `opcodes.c`: bounded bytecode VM, explicit call stack,
  resumable conditions and encrypted message reader, adapted from Sarien.
* `src/next/resource.c`: AGI v2 directory and volume reader using esxDOS file
  calls. Supports bounded flat reads and sequential streams.
* `src/next/resource_cache.c`: 192 KB resource cache, pinned handles, bounded
  copies across banks and least-recently-used eviction. All four types share
  this cache; resource bytes remain unmodified.
* `src/next/platform.c`: MMU windows, packed pixel storage, palette and Layer 2.
* `src/next/view_resource.c`: pinned cache resource adapter for the VIEW reader.
* `src/next/logic_resource.c`: LOGIC I/O adapter and resource preload commands.
* `src/next/scene.c`: clean background snapshot/restore and partial Layer 2 updates.
* `src/next/runtime*.c`: game cycle, banked actor state, movement/cycling,
  text, dictionary/parser, command input, menus and silent sound timing. `bank_calls.c` contains
  resident call gates; `clock.c` supplies a resident IM2 frame counter.
* `src/runtime_main.c`: combined interpreter frontend (`-Runtime`).
* `src/main.c`: temporary viewer shell; it will be replaced by engine startup
  and game selection as the interpreter develops.

The game directory is a single `GAME_DIRECTORY` setting, default `games/sq1`.
The decoder contains no SQ1-specific resource data or room IDs. The viewer
starts at picture 1 and VIEW 1 for the sprite demonstration; no game LOGIC is
executed and the viewer controls are not yet connected to engine state.
`src/logic_main.c` is an earlier minimal startup diagnostic; `-LogicTest`
exercises the VM without graphics. The combined runtime starts LOGIC 0 and
lets the game's scripts choose rooms and actors. See [runtime.md](runtime.md).

## Viewer memory layout

| CPU address / physical pages | Purpose |
| --- | --- |
| `$0000-$3fff` | ROM/esxDOS availability and ROM font |
| `$4000-$5fff` (MMU2) | Window onto packed picture data |
| `$6000-$7fff` (MMU3) | Shared window onto display or resource cache; cache calls restore it |
| `$8000` upward | Code, globals, cache metadata/staging, 4 KB picture buffer, 1 KB fill stack |
| `$ff00` downward | CPU stack |
| 8 KB pages 32–35 | 26,880 packed visual/priority pixels, remaining bytes reserved |
| 8 KB pages 36–39 | Clean picture snapshot for restoring sprite backgrounds |
| 8 KB pages 40–49 | 80 KB Layer 2 framebuffer (16 KB bank 20 onward) |
| 8 KB pages 64–87 | 192 KB raw resource cache, 24 independently allocated pages |

Pages 32–95 are present on the base 1 MB Next; pages 96 onward require expanded
RAM. The earlier claim that the framebuffer occupied physical memory below
400 KB omitted the hardware's reserved 256 KB region. Page numbers here are
Next MMU page numbers, not absolute chip addresses. Extra RAM does not require
a flat C pointer: the platform explicitly switches 8 KB windows. Interrupts remain
disabled in the viewer so the ROM interrupt handler cannot access a remapped
ULA screen. The combined runtime uses a different code layout: resident code
starts at `$a000`, MMU4 maps code overlays in pages 50–56, 92 and 95. Actor records
live in page 60, text/parser/inventory state in 61, picture input in 63 and actor sorting/
redraw/movement scratch in 91. Tests use code pages 57, 88–90 and 93–94. Its resident IM2 handler
increments a tick counter, queues keyboard edges and advances AY playback.
Audio temporarily maps page 91, preserving MMU3 and the NextReg selector.
See [runtime.md](runtime.md) for the complete
layout, overlay pointer rules and reserved interrupt/stack addresses.

## Known limits

* AGI v2 resource layout only; no AGI v3 decompression or SCI support.
* The cache handles 16-bit resource sizes, including the 10,651-byte LOGIC.
  The viewer picture decoder requires a flat 4 KB input buffer; the combined
  runtime provides a dedicated 8 KB bank for contiguous picture input.
  VIEW decoding and VM byte fetching use handles and offsets already.
* VIEW decoding covers DOS/EGA AGI v2 with cel-level mirroring. The AGI 2.230
  loop-header mirroring variant, AGI256 and Apple II layouts are not implemented.
  The sprite viewer handles one controlled actor; the runtime has 256 actors,
  sorting, directed/follow/wander movement, collision and cycle timing.
* Picture commands F0–F8 and the default single-pixel FA brush are supported.
  Larger/textured brushes produce an explicit unsupported result.
* Fill work storage holds 512 pending seeds; overflow is reported.
* Rendering uses C with span writes and bank-aware display copying. Profiling
  and an initial optimization are recorded in `performance.md`; decoding
  still needs substantial improvement before gameplay.
* Picture decoding, collision fixtures and original-room traversal are tested;
  broader game compatibility is still being established.
* Normal NEX/ROM mappings and direct CSpect file access are assumed. Physical
  Next testing, OS memory reservation and exit-to-OS restoration remain work.

## Interpreter strategy

The supplied data has 101 LOGIC resources (largest 10,651 bytes), 73 pictures
(largest 3,921 bytes), 238 VIEW resources (largest 4,927 bytes), and 50 readable
SOUND resources (largest 5,896 bytes). `SNDDIR` entry 0 is `ff23c1`, referring
to an absent `VOL.15`; the inventory flags it rather than altering the files.
Its effect on gameplay must be established when the sound/LOGIC layers exist.
`LOGDIR` MD5 is `5d67630aba008ec5f7f9a6d0a00582f4`, matching the SQ1 2.2 data
in ScummVM's detection table; that hash alone does not identify every original
interpreter revision.

Sarien has been selected for staged C engine adaptation. The pinned source,
license, compiler probe, measured sizes and required adaptations are recorded
in [interpreter-foundation.md](interpreter-foundation.md). The diagnostic VM
now incorporates adapted opcode semantics and metadata, with preserved notices
and root `COPYING`. The combined runtime now links the VM, graphics and actor
commands through banked call gates. The graphics viewer remains separate.

Next milestones:

1. Extend room traversal coverage, including real-game follow/wander encounters.
2. Extend remaining game commands. AY playback, inventory UI and item
   inspection are implemented; see [sound notes](sound-plan.md) and
   [inventory notes](inventory.md).
3. Add save/restore and play through SQ1, extending banked state as required.
4. Exercise a second AGI game before claiming reusable game compatibility.

The [sound notes](sound-plan.md) record the AY backend, interrupt/memory
contract, compatibility checks and measured 28 MHz performance.

## References

* [AGI picture specification, Lance Ewing et al.](https://www.agidev.com/articles/agispec/agispecs-7.html)
* [ScummVM AGI detection table](https://github.com/scummvm/scummvm/blob/master/engines/agi/detection_tables.h)
* [Next memory map](https://wiki.specnext.dev/Memory_map)
* [Next specifications](https://wiki.specnext.dev/Specifications)
* [CX16Agi](https://github.com/wizardmanannan/CX16Agi)
* [Sarien](https://github.com/cmatsuoka/sarien)
* Installed z88dk headers/startup definitions and CSpect `ReadMe.txt`.
