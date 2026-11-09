# Banked runtime checkpoint

`build.ps1 -Runtime` builds `build/runtime.nex`; `run.ps1 -Runtime` runs it
in CSpect at 28 MHz. Original resources default to `games/sq1`; `ZXQUEST.CFG`
can select another directory (`run.ps1 -Runtime -Game kq1` or `-Game pq1`). There are
no embedded SQ1 scripts, pictures, sprites or music. The engine starts LOGIC 0;
room IDs and title choreography come from the supplied scripts.
Normal play uses CSpect's capped 28 MHz timing. Test modes add `-freerun`,
which removes host pacing; their visible animation is faster than normal play.
Performance counters still report emulated 50 Hz frames at 28 MHz.

## Current behaviour

The runtime initializes AGI variables/flags, reads inventory names/locations from
OBJECT, registers script-defined keyboard bindings and menus, and runs timed
LOGIC scans. Room events unwind executing resources, reset scan starts and
actor state, set the first-scan flag, and restart LOGIC 0.

All 256 actor IDs have banked records. Implemented commands include view,
loop/cel, position, draw/erase, priority, update/cycling controls, end/reverse
loop completion flags, step timing and directed/follow/wander movement. The 32-byte actor
records accept offscreen coordinates from `position`/`position.v`; `draw`
adjusts placement before rendering. End/reverse-loop commands restart updates
on stopped actors. Completion stops cycling, clears direction and returns the
cycle mode to normal, so a later `start.cycling` cannot repeat the old completion
flag and reset a game-script timer indefinitely.
The actor
records retain the last drawn position, cel and priority. Drawing restores
only changed areas from the clean picture, expanding through intersecting
actors and connected priority-control runs. It sorts affected actors by
priority/baseline/ID, composites original cels, and uploads the changed area
to Layer 2. New pictures and force.update explicitly rebuild the composition.
Sprite pixels and display fragments use bulk row transfers; a text cell maps
its display bank once. See [performance.md](performance.md) for measurements.
Movement now observes the clean picture's priority/control map along the
entire actor baseline: black always blocks, blue blocks unless ignored, red
sets ego trigger flag 3, and an entirely water-covered baseline sets flag 0.
Water/land restrictions and priority-15 terrain bypass follow AGI v2 rules.
Animated, drawn actors collide at touching/crossing baselines unless either
ignores objects. Script-defined block rectangles stop boundary crossings.
Draw and reposition commands adjust blocked placements using a bounded
west/south/east/north spiral. Reposition suppresses one automatic movement.

Follow/wander decisions run on the actor's step timer and use the same
collision path. Follow uses baseline centres and a completion distance;
blocked actors take bounded random detours. Wander refreshes its direction
after a stall or countdown expiry. `player.control` cancels ego's automatic
motion. See [automatic-motion.md](automatic-motion.md) for semantics and tests.

Movement produces v2 ego edge events and v4/v5 other-actor/edge events for the
next LOGIC scan. Codes are top=1, right=2, bottom=3, left=4. A rejected move
cannot emit an exit; events clear on the next movement tick. Original scripts
choose destination rooms. New rooms reset movement timing, picture validity,
block rectangles and scan starts, and place ego on the opposite edge before
first-scan logic runs. See [collision.md](collision.md).

Text supports display, clear lines, text/graphics modes, cursor, strings,
`%s`/`%v`/`%w`/`%m`/`%g` substitutions and modal print/get.string. Nested message
expansion is limited to four levels; numeric substitutions support zero padding.
Modal print saves the pixels beneath the message before drawing. Enter/Escape
restores only the covered rows, including actors and text, without clearing
or reconstructing the screen. The usual 320x96 box uses 30,720 raw bytes;
long messages extending below it use packed 16-colour pixels within the same
32 KB backing store. Dialogues wrap at word boundaries within 36 columns,
with two-column margins. Authored line breaks, blank paragraphs and spacing
are retained; CRLF counts as one break. Only an individual word longer than
36 characters is split. Layout runs after message substitutions, and the same
layout measures the saved rows and paints the text. The Enter prompt moves
below longer text; it is omitted when text fills the final viewport row.
Messages that exceed the viewport are still clipped.
Returning from text mode
to graphics clears text-mode cells and displays the current picture on a
black background. Repeating `graphics` in graphics mode leaves the display
alone. `show.pic` replaces the picture's 21 text rows and forgets text covered
by that image, retaining status/input margins and the current video mode.
Menus use the game's own
labels and controller numbers, support disabled items, and preserve the
underlying screen. Opening a menu saves the 320x200 viewport in a packed
16-colour backing store. Navigation paints the menu in place and restores
only rows exposed by a shorter menu; closing restores the covered rows
without clearing or rebuilding the screen. Labels are clipped to one line.
A selection is available to the resumed script in its
current scan; closing the menu consumes the key. Normal key bindings are
latched for one scan. Frame interrupts sample key edges into a 31-event queue,
so brief presses during picture decoding or animation redraws survive until
the engine can process them. Held keys enqueue once; a full queue drops new
events. Rendering is still synchronous, so a captured press can be delayed
until the current operation finishes. Function-key/Alt mappings are not yet
provided by the Spectrum keyboard frontend.

Sound resources are validated and converted into a private page-91 buffer.
The resident interrupt player drives AY1 tones and AY2 noise at 60 ticks/second
from the 50 Hz frame clock, including during picture drawing and modal UI.
Foreground code applies completion flags; stop/replacement, flag 9, room changes
and reset silence playback. All 50 supplied readable sounds pass independent
host duration checks. See [sound notes](sound-plan.md) for approximations, limits
and CPU measurements.

Save/restore now uses nine per-game slots and two checksummed generations per
slot. A restore validates and stages the snapshot before replacing game state,
then restarts LOGIC 0 with flag 12. See [save/restore](save-restore.md) and the
[SQ1/KQ1/PQ1 integration results](compatibility.md).

SQ1 reaches title room 67 and its animated logo. Space advances through its
original scripts to room 69's first-name prompt. Submitting a name now enters
room 2, completes the scripted walk out of the closet and enables player control.
The command line submits typed text on the next LOGIC scan. `parse`, `said`,
`word.to.string`, `compare.strings`, `echo.line`, `cancel.line` and input enable/
disable are implemented. A successful `said` consumes that scan's input event;
modal messages retain the suspended scan until Enter. The original unknown-word
response uses `%m`/`%w` substitutions. Score and sound status update automatically.

WORDS.TOK stays separate on disk, with a 512-byte read cache and validated
compressed entries. Matching uses original 16-bit synonym groups, ignored
words, longest phrases, ANYWORD and rest-of-line patterns. Punctuation inside
dictionary entries is preserved. Input is bounded to 80 characters and 20
parsed tokens, with the game's v24 setting limiting interactive input further.
Dictionary state shares the unused tail of text page 61. No vocabulary or
SQ1 command rules are embedded in the interpreter.

Inventory display/selection and item pictures/descriptions now implement
`status`, `show.obj` and `show.obj.v`, including `%n` name substitutions and
paginated description text. See [inventory notes](inventory.md).
Save/restore, other message substitutions and many later
commands remain unimplemented.
Modal UI pauses game time; audio continues. Text uses the Spectrum ROM font; windows are not
a pixel-exact reproduction of Sierra's. Positioned display text, menus and
the command line retain their existing layout.

The runtime returns explicit errors for unsupported commands and invalid
resources. Trace settings and script-size requests are recorded; tracing and
save-screen replay are not implemented. Cache discard requests release no
engine pointers: unpinned immutable resources remain eligible for eviction.

## Code and memory

The resident executable begins at `$a000`. MMU4 (`$8000–$9fff`) maps one 8 KB
code overlay at a time. `src/next/bank_calls.c` saves the previous page on the
CPU stack and restores it on return, including nested VM-to-host callbacks.
All data passed across overlays must remain addressable while the callee is
mapped: resident globals, the CPU stack, or an explicitly managed data window.
Never pass an overlay constant's pointer to a different overlay. Callback
addresses are resident gates. Each translation unit renames its implementation
symbols so internal calls stay within its own bank.

| 8 KB page | Contents |
| --- | --- |
| 32–35 | Packed visual/priority picture and composited actors |
| 36–39 | Clean background; page 39 tail holds text-occlusion masks and derived cel cache |
| 40–49 | Layer 2 framebuffer |
| 50 | LOGIC VM and opcode arities |
| 51 | Picture decoder, save-slot UI, startup, keyboard labels and joystick input |
| 52 | Scene operations, clock/key setup, dialogue and menu layout |
| 53 | Runtime cycle, commands, state access, cold resource loading, LOGIC preload, VIEW setup and cache status |
| 54 | Actor commands, animation, collision and permanent picture stamps |
| 55 | Text, menus and character display |
| 56 | Dictionary, parser, input/status, text-occlusion masks and sound preparation code |
| 57 | Renderer, inventory, motion or sound regression code, selected by test build |
| 58–59, 62 | Modal menu/dialogue backing store (first 24 KB) |
| 60 | 256 actor records, 32-byte stride |
| 61 | Persistent text cells/attributes at `$6000–$67ff`; parser `$6800-$6bff`; inventory metadata `$6c00`; modal text `$7000-$72ff`; save-label cache `$7400-$751f`; raw text `$7e00-$7fff` |
| 63 | Picture input scratch, or final 8 KB of modal backing store |
| 64–87 | 192 KB resource cache; 32 handles; restore temporarily reserves ten unpinned pages |
| 88–90 | Integration-test code only |
| 91 | Actor scratch/baselines at `$6000-$65ff`; private prepared sound at `$6600-$7fff` |
| 92 | Snapshot storage code |
| 93 | VIEW decoder, validated cel cache and actor compositor |
| 94 | Room-transition regression code only |
| 95 | Scripted movement/follow/wander, OBJECT/inventory and host control dialogs |

MMU2 remains the pixel window; MMU3 is shared by resources, actor/text state
and display operations and is restored by each operation. The picture decoder
holds page 63 in MMU3 while reading its input. It uses MMU2 for pixels.
Picture decoding is complete before modal UI opens, and the VM cannot decode
another picture while modal UI is active. The next picture load replaces
the input scratch before decoding. Thus modal backing can share page 63
without additional cache pages. The parser milestone reduces the cache from
32 to 24 pages to reserve test overlays and actor sorting scratch within base
RAM. Menu capture/restoration reuses the resident formatting buffer as staging;
dialogues retain expanded text in page 61 while that same resident buffer stages
pixels. Raw formatting scratch also lives in page 61, with the data mapping
held and restored around formatting. The bank-private helpers live in page 55.

The IM2 table is `$f800–$f900`, with a resident jump at `$f9f9–$f9fb`.
The interrupt increments a resident frame counter and samples the keyboard
into a resident queue, then advances the resident audio player. Audio saves
and restores MMU3 and the NextReg selector, including when interrupting a
select/data port pair. It never calls the VM, overlays, renderer or filesystem. The stack is reserved at `$fb00–$feff`, starting at `$ff00`.
The build rejects resident overlap, overlays over 8 KB, and non-1-MB NEX files.
The October 2026 bank rebalance initially freed 487 resident bytes and 3,401
bytes in page 95. After the control/UI additions, the playable resident image
ends at `$f711` (239 bytes before IM2); page 95 has 2,321 free bytes.
Production overlay usage is:

| Page | Used bytes | Free bytes |
| --- | ---: | ---: |
| 50 | 6,609 | 1,583 |
| 51 | 6,008 | 2,184 |
| 52 | 6,853 | 1,339 |
| 53 | 7,042 | 1,150 |
| 54 | 7,043 | 1,149 |
| 55 | 7,287 | 905 |
| 56 | 5,645 | 2,547 |
| 92 | 6,927 | 1,265 |
| 95 | 5,871 | 2,321 |

Cold LOGIC preload, VIEW open/close and cache-status helpers use page 53;
foreground AY setup uses page 56. Resource-read callbacks, hot cache access,
the keyboard interrupt and audio playback remain resident. Test builds have
their own headroom figures. The build reports every overlay's usage and warns
below 512 free bytes per overlay or 256 resident bytes; hard overlap checks
still reject invalid images.
The separate VM-only diagnostic leaves interrupts disabled and installs no
IM2 table, so it may use resident memory up to the `$fb00` stack boundary.
This remains a CSpect target; physical Next and OS memory ownership/exit
restoration have not been validated.

## Modal VM contract

A command callback may return `VM_WAIT`. The VM retains its explicit call
frames, pinned resources and next-instruction position. Further `run` calls
return WAIT without executing. After UI completion the host calls
`agi_vm_resume`, then continues the same scan. Completion, errors, cancellation
and room changes still release executing pins. Test callbacks cannot suspend.

## Validation and sources

`tools/prepare_runtime_tests.py` writes authored fixtures and independently
computed sound expectations under ignored `build/`. `-RuntimeTest` runs the
actual banked Z80N runtime, checks modal continuations and resource cleanup,
and executes the supplied SQ1 startup and a complete credits/story/title loop.
It also runs all 738 independently decoded vocabulary entries and original
first-room commands. `-ParserTest` builds/runs this last subset independently.
Both include `render-tests.txt`: incremental/full composition comparisons,
priority-control dependencies, untouched display pixels and all ROM glyphs.
The tests export Layer 2 frames for visual inspection under ignored `build/`.
`-MotionTest` adds baseline/control collisions, changing actor footprints,
permanent scenery, all four room-entry placements and unmodified SQ1 travel
between rooms 2, 3 and 1. It writes `motion-tests.txt` and `room-tests.txt`.
See [validation.md](validation.md).

`tools/build_banked.ps1 -TestKerona` runs the supplied SQ1 LOGICs 111, 110,
26 and 27 in staged scenarios. It checks Autonav text, the spider droid's
landing-to-pursuit/wander transitions, the rock/door animation and cave
waterfalls. It writes `build/kerona-tests.txt` and three Layer 2 snapshots.

Cycle, actor and parser semantics are adapted from the pinned Sarien sources with
copyright/GPL notices in the implementations; see [COPYING](../COPYING) and
[provenance](interpreter-foundation.md). Banking, bounded state and the text
frontend are new code. Format/timing references:

* [AGI sound format](https://www.agidev.com/articles/agispec/agispecs-9.html)
* [ScummVM PCjr sound timing](https://github.com/scummvm/scummvm/blob/master/engines/agi/sound_pcjr.cpp)
  (60 Hz timing reference only; no ScummVM implementation code copied).
* Installed z88dk ZX Next startup/memory-map and IM2 headers.
* Pinned Sarien `words.c`, `op_test.c`, `op_cmd.c`, `keyboard.c` and `cycle.c`.
* [AGI Studio WORDS.TOK description](https://agistudio.sourceforge.net/help/wordstok_descr.html).


The keyboard/save update also implements shake.screen with temporary Layer 2
scroll offsets (four presentation ticks per count), restoring the prior scroll
registers without changing packed picture, collision/priority, or permanent text.
Key events can reach have.key inside an executing LOGIC scan; the test consumes
both the live and pending event to avoid replaying the same key next scan.
Text-screen input is not sent through the command-line parser. This allows the
original escape-pod monitor's polling loop to return to graphics on any key.
Cold platform initialization/full-picture presentation/clock setup now share
page 52; console font rendering shares page 55. Console call gates stage caller
strings on the stack before switching code banks. Resident end is `$f562`.


### November display/performance build

The 9 November executable remains a base-1-MB NEX. Resident end is $f600,
leaving 512 bytes before reserved IM2/stack. Overlay free bytes: page 50 1,583;
51 2,224; 52 2,640; 53 916; 54 1,013; 55 424; 56 698; 92 3,277;
93 1,741; 95 2,194. Page 55 remains the tightest overlay.

VIEW decoding/caching and composition now share page 93. Motion regression
code moved from page 93 to page 90; production controls and sound retain their
prior mappings. Text visibility helpers share page 56. See view-rendering.md
for the reserved page-39 tail and save compatibility, and performance.md for
measured 28 MHz results.
