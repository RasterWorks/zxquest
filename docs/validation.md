# Milestone 1 validation — 23 October 2026

## Bank rebalance — 25 October 2026

Production builds with resident end `$f619` (487 free bytes before IM2)
and page 95 using 4,791 bytes (3,401 free). All production overlays have
at least 1,015 free bytes. See runtime.md for the updated allocation table.

Fresh uncapped CSpect diagnostics after the rebalance:

- UI: 13 checks, zero failures.
- Motion: 60 checks, zero failures; original closet walk has 22 cel changes
  over 23 samples and returns player control at the destination.
- Sound: 41 core checks and two original-game checks, zero failures;
  all 50 original SQ1 sounds complete with expected flags/durations.
- Save: 44 checks, zero failures, using disposable `build/save-sd` files.
- Compatibility: SQ1, KQ1 and PQ1 opening/movement/parser/save/restore
  checks, plus SQ1 pod and explosion paths, zero failures.

Updated stale test inputs to use an unassigned letter instead of reserved
Tab-Q for prefix cancellation, and to confirm the newer save/restore
dialogue stages. No keyboard behaviour changed in this rebalance.
Physical Next validation and full-game playthroughs remain outstanding.

Environment: Windows, z88dk v23854-4d530b6eb7-20251002, CSpect 3.3.1.0.
Target: 28 MHz Z80N, 1 MB NEX, external AGI v2 game data.

The CSpect test executable completed with:

```text
SUMMARY pictures=73 failures=0
```

All twelve synthetic/integration checks passed: enclosed visual/priority fill,
full-screen priority fill across MMU banks, no-op white fill, positive and
negative line midpoint rounding, truncated coordinates, missing terminator,
unknown opcode, out-of-bounds coordinate, unsupported brush reporting,
resource capacity enforcement and missing game directory reporting.

All 73 existing SQ1 pictures decoded without an error or fill-stack overflow.
Several rooms and the title picture were visually inspected in CSpect. This
does not establish pixel-perfect equivalence with Sierra's interpreter.

The initial build's resident code/data ended at `$b93a`, below the reserved
stack at `$fb00-$feff`; the initial stack pointer is `$ff00`. The build checks
the resident end address and verifies the NEX's 1 MB memory requirement.

The test program exported picture 1 from the emulated Next to
`build/pic001.raw`; `tools/render_dump.py` produced `build/pic001.png`.
These generated files and the raw test log are intentionally not tracked.

The user manually confirmed that N, B and P all work in CSpect. Next/back
changes were slow; toggling the already decoded priority map was faster.
Automated key injection did not activate CSpect's own F1 shortcut or viewer
controls, so keyboard validation is manual and separate from the pass count.

The game inventory reports one unresolved issue: `SNDDIR` resource 0 points
to a missing `VOL.15`. All picture resources are present. No game files were
modified. Sound is outside this milestone and needs follow-up.

## Performance revision

After span-write and display-copy optimizations, all 73 pictures matched the
frozen original decoder byte for byte (both visual and priority nibbles).
All 18 named checks passed: the original 12 plus full Layer 2 visual/priority
comparisons for pictures 1, 27 and 67. The final summary remained
`SUMMARY pictures=73 failures=0`.

The optimized viewer ends resident code/data at `$ba37`; the larger regression
test ends at `$cde9`. Both remain below the reserved stack. Measurements for
four pictures show 29–32% less clear/decode/display time and a display copy
time reduced from about 1.1 seconds to 0.34 seconds. See `performance.md`.

## Banked resource milestone

The dedicated resource test NEX completed in CSpect with:

```text
SUMMARY resources=462 checks=33 failures=0
```

The host fixture tool parsed the original directory/volume files independently
and computed CRC-16/CCITT expectations using Python's `binascii.crc_hqx`.
The emulated Next loaded each resource through the banked cache, copied every
byte back in chunks and calculated CRCs using a separate C implementation.
All 101 LOGIC, 73 PICTURE, 238 VIEW and 50 readable SOUND resources matched
their expected lengths and CRCs. All successful acquisitions also had a
cache-hit/release check. The existing missing `VOL.15` reference was reported
as the expected `RES_IO`; it is not included in the 462 readable resources.

The 33 named checks cover empty, 8 KB boundary and 65,535-byte resources;
single reads crossing banks; overflow-safe bounds; corrupt/truncated headers
and payloads; repeated failed-load cleanup; type-aware cache identity;
full-cache pin protection; fragmented allocation; LRU eviction; stale handles;
game switching; pin-count overflow; legacy flat-loader size limits; MMU2/MMU3
restoration; and picture/display canaries. Thirty-two pinned 8 KB resources
exercise all 256 KB of the pool, including page 95 on the 1 MB target.

The final viewer ends resident code/data at `$c89b` and the resource test at
`$dc28`; both pass the build's `$fb00` stack boundary and 1 MB NEX checks.
The viewer using the new cache was launched and its picture 1 display inspected
in CSpect. Picture algorithms were unchanged in this milestone; the expensive
73-picture decoder regression was not repeated. Keyboard behavior was not
retested automatically; previous manual validation remains the evidence.

SHA-256 comparison before and after the resource runs confirmed all 20 game
files unchanged. Fixtures, checksums, logs, binaries and upstream probe files
remain generated/ignored content under `build/`.

The reproducible Sarien compiler probe also completed: `logic.c`, `op_cmd.c`,
`op_test.c` and `motion.c` compile and assemble to Z80 objects after the
documented portability changes. Compiler warnings and memory limits are
recorded in `interpreter-foundation.md`. This is not a linked/running VM test.

## VIEW and compositor milestone

The dedicated VIEW test NEX completed in CSpect with:

```text
SUMMARY views=238 cels=1652 mirrored=216 checks=37 failures=0
```

All cels matched the independent host decoder's dimensions, transparent colour,
orientation and decoded CRC-16. This includes 675,588 decoded pixels and all
216 mirrored cels. Expectations are external generated test data, not game
assets incorporated into the interpreter. See `view-rendering.md` for the
format references, reader contract and scope of compatibility.

The 37 checks include invalid headers/tables/indexes, truncated and overlong
RLE, bank-crossing headers and cel data, offset overflow, transparency,
occlusion, equal-priority ordering, control-line preservation and bottom-edge
termination, clipping, malformed-draw atomicity, background restoration and
Layer 2 updates. Full room framebuffer comparisons verified small rectangle
updates in visual and priority modes and movement without leftover pixels;
a HUD canary remained unchanged.

The test exported `build/sprite-room.raw`, with original picture 1 and Roger's
VIEW 1 composited on the emulated Next. `tools/render_dump.py` produced the
inspection image `build/sprite-room.png`. The normal viewer was also visually
inspected in CSpect, and the user confirmed that holding WASD moves Roger
without trails and C, L and H work. No automatic keyboard-injection claim is
made; that part is manual validation.

The viewer ends resident code/data at `$dd02` (23,810 bytes from `$8000`), and
the VIEW test at `$ef99`. Both pass the 1 MB NEX and `$fb00` stack boundary
checks. SDCC emits an optimizer-flow warning in the row-mirroring loop;
all mirrored output cases were verified. The unchanged picture algorithms
and resource cache were not subjected to redundant full regressions in this
milestone. All 20 supplied game files still match their earlier SHA-256 hashes.

This validates sprite decoding and compositing, not a playable game. The
viewer moves a single actor with diagnostic controls. LOGIC execution,
collision, the engine object table and real AGI timing are still pending.

## LOGIC VM foundation

The final LOGIC test NEX completed in CSpect with:

```text
SQ1 startup status=10 logic=104 pc=8 opcode=96 test=0 v10=2
SUMMARY logics=101 messages=2137 bytes=100607 checks=58 failures=0
```

The 58 named checks cover arithmetic limits and 16-bit multiplication,
indirection, all six comparisons and both flag tests, AND/OR/NOT short-circuit
behaviour, one-token execution budgets, signed loops, nested calls, scan
starts, room-event unwinding, cancellation and explicit errors. Malformed
cases include headers, operands, conditions, said lists, message tables and
offsets, division by zero and branches outside the code section. The suite
also checks 16-deep recursive-call cleanup, read failures, command/test error
locations, cache eviction while parent and child execute, and instructions
whose operands cross an 8 KB bank boundary. MMU2/MMU3 and a 16-byte canary
at the bottom of the reserved CPU stack remain intact.

The independent Python fixture tool structurally parsed every supplied LOGIC
and checked all branch targets against command boundaries. It decrypted the
message sections and computed per-LOGIC CRC-16 values including each message's
NUL terminator. The Z80N implementation independently read all 2,137 messages
through the banked cache, in small chunks, and matched all 100,607 plaintext
bytes by count and CRC. These are message checks, not evidence that every
gameplay branch or command has executed.

Unmodified SQ1 LOGIC 0 executes its startup condition, assigns variable 10 to
2 and calls LOGIC 104. Execution stops explicitly at unsupported `trace.info`
(`$96`), resource offset 8. Starting room 1 with its first-scan flag reaches
unsupported `set.horizon` (`$3f`), also at offset 8. These are deliberate
compatibility checkpoints; missing commands are not silently skipped. The
standalone diagnostic and final passing test screen were inspected in CSpect.

An initial test run had one harness failure: the preload test left variable
0 set to 1 before the startup test. Resetting VM state between those cases
fixed the test setup; the final run above is from the corrected executable.

The final diagnostic ends resident code/data at `$cb59` (19,289 bytes), and
the test build at `$ed2d` (27,949 bytes). Both pass the 1 MB NEX and `$fb00`
stack-boundary checks. The resident VM instance occupies 1,070 bytes. The
VM targets use SDCC's 20,000-node register-allocation search limit to keep
compilation practical; the existing graphics build settings are unchanged.
These sizes are for separate binaries. The combined game engine still needs
code/state banking work before all graphics and gameplay commands can fit.

All 20 game files match the earlier SHA-256 baseline. Picture, VIEW and cache
implementations are unchanged, so their full earlier regressions were not
repeated. The default sprite viewer remains separate. See `logic-vm.md` for
reproduction, API contracts, source provenance and remaining integration.

## Combined banked runtime

The final integration test completed in CSpect:

```text
SQ1 first scan r=0 room=67 logic=0 pc=4176 op=00
SQ1 input r=14 room=69 ui=3 logic=69 pc=25 op=73
SUMMARY checks=43 failures=0
```

The authored checks exercise encrypted OBJECT inventory locations, nested
bank restoration, script-driven pictures/actors, all 256 actor IDs, sorting,
overlapping erase and unanimate.all cleanup, end/reverse-loop flags, directed
movement and step restoration. Keyboard/controller events last one scan.
Nested print preserves two call frames and resource pins while waiting;
Enter resumes after the instruction and releases both pins on completion.
Get.string handles editing and deletion. Menu tests cover disabled items,
current-scan event delivery, key consumption and resumed-script behaviour.

Sound tests check malformed offsets, fractional 60 Hz timing from 50 Hz
frames, stop/completion flags, and independent duration totals for all 50
readable original sounds. This validates timing only; the runtime is silent.
Clock carry, room lifecycle, a resident IM2 interrupt while overlay code is
mapped, and a 16-byte stack boundary canary also pass.

Actual SQ1 LOGIC 0/104 initializes the runtime and enters title room 67.
The original title script draws and cycles the logo. The game's Escape
binding opens its five menus; navigating to File and closing consumes Escape
without accidentally skipping the title. Space then reaches room 69 and
suspends at get.string for the first-name prompt. The test exports original
title and animated-title packed pixels; `tools/render_dump.py` renders these
as inspection artifacts, never inputs to the interpreter.

The banked core VM regression was also rerun after adding `VM_WAIT`:

```text
SUMMARY logics=101 messages=2137 bytes=100607 checks=58 failures=0
```

Core code in page 50 occupies 6,664 bytes. Other runtime overlays occupy
2,055 (page 51), 3,284 (52), 4,102 (53), 4,368 (54), and 3,914 bytes (55).
The runtime test overlay is 7,046 bytes in page 56. All are below 8 KB.
The production resident image ends at `$f573` (21,875 bytes from `$a000`),
and the runtime test at `$f418`, below the `$f800` interrupt region. Both NEX
files target base 1 MB. Code/data/stack and overlay limits are enforced by
the build.

The interactive production title, later credits, and test name-entry screen were
visually inspected using the Computer Use skill. Keyboard behaviour beyond
the runtime event interface still needs the user's manual confirmation.
The test does not validate a complete unattended intro, gameplay, collision,
audio output, save/restore or other Sierra games. The next known unsupported
instruction after submitting the name is parse (`$75`). All 20 supplied files
still match the earlier SHA-256 baseline. No game files were changed.

## User-reported menu/input and startup fixes

Manual testing confirmed Shift+1/5/6/7/8 and name entry work, and Enter reaches
the expected `parse` stop (LOGIC 69, offset 31, opcode `$75`). The user then
reported the long startup wait, missed menu presses during the title glow,
and a menu bar left visible after closing. Those reports prompted bulk fill/
scene transfers, interrupt-sampled key events and full-screen menu restoration.

The final CSpect runs completed with:

```text
SUMMARY pictures=73 failures=0
SUMMARY views=238 cels=1652 mirrored=216 checks=37 failures=0
SUMMARY checks=49 failures=0
```

All 73 complete packed visual/priority buffers match the frozen previous
decoder. All 1,652 original cels, 216 mirrored cases and 37 VIEW/compositor
checks pass, including bank-crossing restoration, clipping, dirty rectangles
and preservation of HUD pixels during scene updates.

The runtime suite adds five menu/input checks and an original title-glow
case. Entire 80 KB framebuffer CRCs match before/after menu close and selection,
with picture origins at text lines 0, 1 and 4, plus text mode. Key press/hold/
release sampling preserves `7,8` across redraw with no duplicate held event;
the 31-entry queue preserves order, bounds overflow and remains reusable.
An enqueued Shift+1 opens the game's menu while the title-glow actor is drawn
and moving. The stack guard still passes with keyboard sampling in the IRQ.
Detailed CRC/key evidence is in `build/runtime-feedback.txt`.

An initial run falsely reported four new failures: generated code omitted
updates to the helper's stack-local result bitmask. The raw screen CRCs and
key values were correct. Keeping the accumulator in explicit static storage
works around that observed toolchain optimization issue; the final zero-failure
run above uses the corrected helper.

Startup CPU time changed from 614 to 297 emulated 50 Hz frames (12.28 to
5.94 seconds). The following 24 scans changed from 335 to 299 frames, with a
53-frame maximum in both builds. Input is retained during those operations,
but redraws are still synchronous. See [performance.md](performance.md).

The production resident image ends at `$f751`, 175 bytes below the reserved
`$f800` interrupt table; the test ends at `$f69a`. Code overlays remain below
8 KB and both targets retain the base 1 MB NEX requirement. Further resident
growth will require moving more code/data into banks. All 20 original game
files still match their baseline hashes. The updated interactive build was
reopened for the user's keyboard check.

## Menu black-flash fix

The user confirmed short menu key presses and complete menu-bar removal,
but observed the full display clearing to black during restoration. Menus
now capture the visible 320x200 AGI viewport as 32,000 packed bytes, redraw
in place, and restore only covered rows. Switching to a shorter menu also
restores the exposed item rows. Page 63 is shared with picture input scratch
only while the VM is suspended; the immutable resource cache is unchanged.

The updated CSpect runtime suite completed with:

```text
SUMMARY checks=50 failures=0
```

The five full-frame CRC comparisons still match after close/selection,
including picture origins at lines 0, 1 and 4 and text mode. These checks
now include deliberate pixels absent from the picture/text model, requiring
restoration from the actual displayed image. Additional sentinels verify that
opening, moving the selection, and switching between three-item and one-item
menus leave uncovered pixels untouched, including the gap below the menu bar
and screen borders. MMU2/3/4 mappings are preserved. Original SQ1 title/glow
menus, script continuation, keyboard queue, timer and stack checks still pass.

Overlay 55 now occupies 4,666 bytes. Production resident data ends at `$f752`
(174 bytes before the interrupt region); the test ends at `$f69c`. The packed
backing uses pages 58, 59, 62 and 63, reusing the existing formatting buffer
for staging. Both builds satisfy the enforced base 1 MB and overlay limits.
All 20 game files still match their original SHA-256 baseline. This run checks
pixel preservation in the actual emulator; the user can assess the perceived
transition in the restarted interactive build.

## Intro story text leaking into credits

The supplied screenshot showed white story text behind the red credits after
the intro loop restarted. The original LOGIC 68 uses `text.screen`, displays
three pages, and returns to LOGIC 67. On re-entry, `graphics` must discard
the text-mode screen. Our implementation instead called `rt_redraw`, which
replayed its saved story cells. `show.pic` also replayed text over the picture
that should replace it. The corrected commands follow the display/mode
behaviour in the pinned Sarien `op_cmd.c` and `picture.c` sources.

Two additional regressions were first run against the previous runtime:

```text
FAIL graphics/show.pic remove covered text and retain HUD
FAIL full SQ1 intro loop clears story text before credits
SUMMARY checks=52 failures=2
```

All 50 earlier checks passed. The full intro ran the original scripts through
all three story pages and back into the credits (1,599 scans),
without skipping or changing script variables. At the graphics transition,
the actual framebuffer CRC was `c454`; removing stale text produced `1100`.
The failing log is preserved under ignored `build/runtime-before-transition-fix-*.txt`.

The direct transition checks require a graphics-mode switch to remove old
story cells, repeated `graphics` to retain current graphics-mode text, and
`show.pic` to remove covered text while preserving status/footer pixels.
A subsequent redraw must not resurrect the removed text. The full intro
exports `build/runtime-intro-story.l2` and `build/runtime-intro-credits.l2`.
Render either with `tools/render_dump.py INPUT OUTPUT --layer2` for inspection.

The corrected runtime completed the identical run with:

```text
intro graphics return at cycle 1441 CRC=1100 clean=1100
intro cycles=1599 stages=15 clean=1 result=0 room=67 logic=0 pc=4176 op=00
SUMMARY checks=52 failures=0
```

The exported final story page and subsequent credits were rendered and
visually inspected: each contains its own text/graphics without remnants of
the other. Menu preservation/navigation, brief key capture during the glow,
name-entry progression and the stack guard still pass. All 20 original game
files retain their baseline hashes. Production still ends at `$f752`, with
overlays 53/55 at 4,170/4,697 bytes. Test overlays 56/57 occupy 8,046/3,216
bytes; the test resident end is `$f69d`. Both binaries remain within their
enforced base 1 MB, overlay and interrupt-region limits.

## Parser and first-room commands — 24 October 2026

The banked runtime now reads the original WORDS.TOK, implements `parse` and
`said`, and advances through the name prompt into room 2 without altering
game scripts or variables to skip gameplay. Typed commands use the original
LOGIC handlers. Status/input rendering, string comparison, word-to-string,
nested message/parsed-word formatting and block commands support this path.

CSpect completed the rebuilt suites with:

```text
runtime-tests.txt:  SUMMARY checks=53 failures=0
parser-tests.txt:   SUMMARY checks=36 failures=0
resource-tests.txt: SUMMARY resources=462 checks=33 failures=0
```

The parser checks include all 738 original dictionary entries against an
independent host expansion of their synonym IDs, ignored words, punctuation,
multiword phrases, ANYWORD/rest-of-line, failed and consumed `said` patterns,
unknown-word reporting, editing, echo/cancel, input flag lifetime, malformed
dictionary data and bounded recursive formatting. The full vocabulary sweep
initially caught incorrect punctuation stripping in entries such as `i.d.`;
the final implementation preserves punctuation for dictionary matching.
Four actor checks exercise block entry/exit, ignore/observe and unblock.

The end-to-end sequence enters a name, waits for the closet-exit movement to
finish, types `look`, closes the original message, types `look gibberish`,
closes the unknown-word explanation, and submits `look door`. Each message
suspends and resumes the original LOGIC scan. Exported `runtime-room2.l2`,
`runtime-look.l2` and `runtime-unknown.l2` were rendered and visually inspected.
The command line and status bar are visible, and the unknown-word message
contains the submitted word. The initial `look` response takes about 111
emulated 50 Hz frames including message drawing; rendering still needs work.

All previous checks pass, including 1,599 scans through the complete intro,
story-to-credits cleanup (`CRC=1100`, matching the clean screen), menu pixel
preservation, short key capture during the glow, bank restoration and the
stack guard. All 20 original files retain their baseline SHA-256 hashes.

Parser code occupies page 56 (3,740 bytes); its data uses the spare tail of
page 61. Moving 768 bytes of actor sorting scratch to page 91 frees resident
space. The immutable cache now uses 24 pages (192 KB), with 32 handle entries;
the rebuilt resource suite exercises the entire configured pool, eviction,
fragmentation and pinning. Test code moves to pages 88–90 (8,094/3,216/5,760
bytes). Production resident data ends at `$f617`, 489 bytes before the IM2
region; the combined test ends at `$f64c`. All overlays remain below 8 KB
and the NEX headers still select base 1 MB RAM.

This establishes command handling in the opening room. Priority/object
collision, border events, later missing commands, audio output and save/restore
remain incomplete; this is not a full-game compatibility result.

## First-room performance regression — 24 October 2026

The reported one-update-per-second slowdown was reproduced before changing
rendering. Stage counters measured 769 emulated 50 Hz frames for 16 idle
scans, including 574 rendering frames. The final build takes 178 frames for
the same idle sequence (about 4.3 times faster). The original `look` command
and message drop from 111 to 28 frames. A completed 16-scan walking sequence
takes 257 frames, excluding acknowledged timed messages; it remains slower
than normal gameplay. See [performance.md](performance.md) for the method.

All rebuilt target suites completed in CSpect:

```text
runtime-tests.txt:  SUMMARY checks=53 failures=0
parser-tests.txt:   SUMMARY checks=37 failures=0
render-tests.txt:   SUMMARY checks=16 failures=0
resource-tests.txt: SUMMARY resources=462 checks=33 failures=0
view-tests.txt:     SUMMARY views=238 cels=1652 mirrored=216 checks=37 failures=0
logic-tests.txt:    SUMMARY logics=101 messages=2137 bytes=100607 checks=58 failures=0
```

New rendering tests compare both packed visual/priority pixels and Layer 2
against a forced full recomposition after moves, overlap, cel/loop/priority
changes, erasing and picture replacement. They cover control-line dependency
runs, preserve a sentinel pixel outside the affected area, compare every
uploaded picture pixel, and check all 96 ROM glyphs with varied foreground/
background colours. The VIEW suite additionally covers clipping, malformed
cels, transparency and priorities. The VM suite retains operand truncation,
window/bank boundaries, nested calls and stack-canary checks.

The full 1,599-scan intro still returns from story text to clean credits with
matching `1100` CRCs. Menu restoration, keyboard queue and nested-overlay stack
guards pass. The room, `look` and unknown-word framebuffer exports match the
pre-optimization frames byte for byte. All 20 original game files retain their
baseline hashes. The final walking frame is exported as `runtime-walking.l2`.

Production resident data ends at `$f6fc`, 260 bytes before IM2; the combined
runtime test ends at `$f7c1`. All overlays fit 8 KB and all NEX files retain
the 1 MB target. Actor records now use their complete 32-byte stride. Renderer
tests occupy page 57 (2,839 bytes); production leaves that page unused. The
separate banked VM-only test ends at `$f885`: it installs no IM2 handler and
leaves interrupts disabled, using its actual `$fb00` stack boundary. Its
stack canary passes. Runtime build guards continue to enforce `$f800`.

## Collision and room transitions — 24 October 2026

Fresh CSpect runs of `-MotionTest` completed:

```text
motion-tests.txt: SUMMARY checks=56 failures=0
room-tests.txt:   SUMMARY checks=28 failures=0
```

The collision suite covers black/blue barriers, the full foot width, water
and trigger flags, clean-map reads across an 8 KB boundary, ignore controls,
touching/crossing actor baselines, changing loop/cel footprints, all edge
codes, step timing, bounded placement, reposition and permanent scenery.
Stamped collision margins participate in movement and existing actors are
excluded from the clean picture snapshot.

Authored room scripts verify opposite-edge placement on all four sides,
immediate/variable room commands, previous-room/ego-view variables, first-scan
flags, scan-start resets, movement timing and released resource pins. Original
SQ1 scripts complete the name/closet sequence, walk east from room 2 to room 3,
accept a typed command, return to room 2, open the archive door into room 1,
and return to room 2. No forced room changes or teleports are used in the SQ1
traversal. Exports `collision-room1.l2`, `collision-room2.l2` and
`collision-room3.l2` provide visual evidence; corridor/archive PNGs were inspected.

Production still ends at `$f6fc` and the full runtime test at `$f7c1`; no
resident-memory growth was needed. Actor/collision/stamp code uses 7,029 bytes
in page 54; the compositor uses 1,993 in page 92. Dedicated test code uses
pages 93–94 (7,106/3,031 bytes). The motion-test resident end is `$f581`.
Every overlay fits 8 KB and all builds retain the base 1 MB NEX target.
All 20 original game files match their baseline SHA-256 hashes.

The rebuilt full runtime regression also completed:

```text
runtime-tests.txt: SUMMARY checks=53 failures=0
parser-tests.txt:  SUMMARY checks=37 failures=0
render-tests.txt:  SUMMARY checks=16 failures=0
```

The screen-restoration fixture now loads its own picture after the preceding
new-room test, which correctly invalidates the previous picture. The full
1,599-scan intro, matching `1100` story-to-credits CRCs, keyboard queue, menu
restoration, bank mappings and nested-call stack guard all pass. Collision
checks add modest CPU work; timings are recorded in `performance.md`.

## Dialogue restoration — 24 October 2026

The user reported slow screen redraw after closing messages and slower play
than the test runs. Normal CSpect play remains capped at 28 MHz, as requested;
test modes use uncapped host pacing. Dialogue close now restores the saved
screen region instead of clearing/rebuilding the entire display.

The completed full runtime run reports:

```text
runtime-tests.txt: SUMMARY checks=53 failures=0
parser-tests.txt:  SUMMARY checks=37 failures=0
render-tests.txt:  SUMMARY checks=16 failures=0
```

Six new full-frame CRC comparisons pass in the feedback suite: graphics
mode, text mode using variable print, a shifted picture, a long multiline
message, bottom-clipped text, and automatic wrapping. Enter and Escape close
the window; unrelated keys leave it active. Deliberate pixels missing from
the picture/text model require actual captured-pixel restoration. Short
messages preserve all eight pixel bits, while extended messages use the
game's 16-colour palette. Outside pixels and MMU2/3/4 mappings remain intact.

Normal message-close timings are four emulated frames (80 ms), compared with
30 (600 ms) before the change. Opening the original `look` response increases
from 29 to 33 frames to save the background. Long-message fixture closes
take 20–26 frames. Detailed CRCs/timings are in `build/runtime-feedback.txt`;
the before/after parser logs are retained under `build/`. See
[performance.md](performance.md) for the measurement boundaries.

An initial shared capture/restore loop exposed a toolchain miscompilation of
the second-column restore address. Separate capture and restore functions
avoid it. The final run above includes all five existing menu CRC checks,
navigation, bank preservation, keyboard queue and stack canary. Original
scripts complete all 1,599 intro scans, returning to credits with matching
`1100` framebuffer CRCs, and resume first-room commands after messages.

Page 55 uses 6,087 bytes; production resident data ends at `$f6fd` and the
full runtime test at `$f7c2`. The extra state is one byte for the last saved
text row. Dialogues reuse the existing 32 KB modal backing pages and raw
message scratch. All build memory checks pass for the base 1 MB target.
All 20 supplied game files retain their baseline SHA-256 hashes.

## Alien-death alarm timing investigation — 24 October 2026

A controlled comparison was added to the parser harness after the original
first-room commands and movement benchmark. It compares input enabled,
disabled, re-enabled, then the original settled-death script path with input
still enabled and the same actors. This deliberately selects SQ1's death
state to isolate its cost; it is not an end-to-end alien shooting test.

The fresh CSpect run completed with:

```text
parser-tests.txt: SUMMARY checks=38 failures=0
render-tests.txt: SUMMARY checks=16 failures=0
```

Input on/off/on each took 218 emulated frames for 16 updates. The death path
took 130, with VM cost falling from 106–107 to 20 frames while actor/render
costs remained essentially unchanged. See [performance.md](performance.md).
The production interpreter and game files were unchanged. Only the diagnostic
emulator was closed afterwards, preserving the user's existing 28 MHz session.

## VM fetch optimization — 24 October 2026

The VM now uses a current-frame cursor for its 64-byte fetch window and retains
the frame pointer across ordinary instructions. Calls/returns invalidate the
window; branches and `said` skips seek or invalidate it. New authored fixtures
exercise operands crossing the window boundary, both outcomes of an IF whose
skip offset crosses that boundary, and a crossing `said` list containing bytes
that also represent condition delimiters. All run with a one-token budget to
exercise cursor preservation across yields.

Fresh CSpect runs of the banked core VM, standalone parser and complete runtime
diagnostics report:

```text
logic-tests.txt:   SUMMARY logics=101 messages=2137 bytes=100607 checks=61 failures=0
runtime-tests.txt: SUMMARY checks=53 failures=0
parser-tests.txt:  SUMMARY checks=38 failures=0
render-tests.txt:  SUMMARY checks=16 failures=0
motion-tests.txt:  SUMMARY checks=56 failures=0
room-tests.txt:    SUMMARY checks=28 failures=0
```

Existing checks cover nested calls/returns, branches, malformed scripts,
short-circuit conditions, scan starts, modal resume, resource pins and stack
boundaries. The original full intro completes 1,599 scans with matching `1100`
story-to-credits framebuffer CRCs. All five menu and six dialogue restoration
comparisons match. Normal dialogue close remains four frames; the long/wrapped
fixtures take 22/26/21 frames in this run. All 738 vocabulary entries pass.

The opening-room, `look`, unknown-word and walking framebuffer exports match
their pre-change SHA-256 hashes exactly. Script CPU cost falls about 20–24%,
with overall idle/walking throughput improving about 13%/9%. Measurement scope
and before/after numbers are in [performance.md](performance.md). This does not
change game timing settings or eliminate the lighter workload after death.
The collision suite and all four authored entry directions also pass. Original
SQ1 scripts complete the closet exit and walk rooms 2 → 3 → 2 → 1 → 2,
including the automatic archive door and a parser command in room 3.

The VM instance is 1,067 bytes, three smaller than before, and page 50 uses
6,609 bytes, down from 6,912. Resident ends are `$f6fa` for the playable build,
`$f7bf` for the full runtime suite, `$f6a3` for the parser suite, `$f57f` for
the motion suite, and `$f9cb` for the VM-only diagnostic (which installs no
IM2 table). Every overlay fits 8 KB; all NEX files retain the base 1 MB target.
All 20 supplied game files retain their baseline SHA-256 hashes.

Ignored evidence copies include `vm-cursor-logic.txt`, `parser-before-vm.txt`,
`parser-after-vm.txt`, `runtime-vm-cursor-tests.txt`,
`runtime-vm-cursor-feedback.txt` and `parser-vm-cursor-runtime.txt` under `build/`.

## Sprite composition and display-copy optimization — 24 October 2026

Sprite rows now advance pointers and compare packed priorities without repeated
per-pixel shifts. Layer 2 fragments use separate Z80 visual/priority loops, with
a 16-byte staging buffer and one-byte count. Source validation, clipping,
priority look-down, control nibbles and actor ordering retain their semantics.

Fresh CSpect runs report:

```text
view-tests.txt:    SUMMARY views=238 cels=1652 mirrored=216 checks=40 failures=0
runtime-tests.txt: SUMMARY checks=53 failures=0
parser-tests.txt:  SUMMARY checks=38 failures=0
render-tests.txt:  SUMMARY checks=16 failures=0
motion-tests.txt:  SUMMARY checks=56 failures=0
room-tests.txt:    SUMMARY checks=28 failures=0
```

Three new VIEW checks cover an independent pixel-at-a-time priority compositor
and complete visual/priority display comparisons. Eight sprite cases include
widths 4/16/17/40, transparent rows, repeated control runs, bottom fallback,
mirroring, clipping and bank crossings. Upload checks use shifted origins,
oversized clipped bounds and varying fragment widths through screen row 255;
MMU3 must be preserved. Existing checks retain malformed-cel atomicity,
occlusion, water, equal-priority overlaps and untouched HUD pixels. The VIEW
test build now uses the runtime's bounded compiler register-allocation budget.

Full runtime checks pass with interrupts active, including the stack guard,
five menu CRCs, six dialogue CRCs and all 1,599 intro scans. Story-to-credits
CRCs remain `1100`/`1100`. Normal dialogue close stays at four frames; long
fixtures measured 23/26/20. Collision checks and original SQ1 travel through
rooms 2 → 3 → 2 → 1 → 2 all pass.

All four original first-room/command/walking framebuffer exports match the
previous build's SHA-256 hashes. Walking costs 220 versus 251 emulated frames
for 16 updates, about 14% higher throughput; its render stage falls from 128
to 96 frames. Idle costs 163 versus 172. See [performance.md](performance.md)
for measurement scope and retained before/after logs.

Page 52 shrinks from 3,391 to 3,281 bytes. The staging-buffer change saves
143 resident bytes: playable resident end `$f66b`, full runtime `$f730`,
parser `$f614`, motion `$f4f0`. The standalone VIEW diagnostic ends at `$f99f`
and installs no IM2 table. All build memory checks pass for the base 1 MB
target. All 20 original game files retain their baseline hashes.

Evidence is retained under ignored `build/` as `view-render-optimized-tests.txt`,
`runtime-render-optimized-tests.txt`, `runtime-render-optimized-feedback.txt`,
`parser-render-optimized-runtime.txt`, `motion-render-optimized-tests.txt` and
`room-render-optimized-tests.txt`. Source/frame snapshots are in `render-before/`.

## Dialogue word wrapping — 24 October 2026

Modal print now lays out expanded text in 36 columns with two-column margins.
It moves complete words to the next line, splits only words wider than a full
line, and preserves explicit LF/CR/CRLF breaks, paragraphs and indentation.
The same routine measures and draws the layout. Long boxes clear and save all
covered rows; their Enter prompt follows the text instead of overwriting row
18. If text reaches row 24, the prompt is omitted. Viewport clipping remains.
Positioned display text and the original game resources are unchanged.

Eight new CSpect framebuffer comparisons use hand-wrapped expected lines:
ordinary prose, an exactly full line followed by CRLF, a 76-character word,
nested message/string substitutions, indentation/repeated spaces/CR, empty
text, a longer box with its footer below the text, and an exactly full line
followed by a space. All eight match the reference pixels and restore the
original complete 80 KB framebuffer, foreground/background and MMU mappings.
Actual short and long outputs were also rendered from emulator dumps and
visually inspected (`runtime-wrap.png` and `runtime-wrap-long.png`).

All five menu CRCs and six dialogue CRCs match. Short dialogue dismissal
remains four emulated frames at 28 MHz; extended fixtures take 23/26/23 frames.
The 1,599-scan original intro retains matching `1100` transition CRCs. The
long-dialogue fixtures use 16-colour pixels throughout the extended saved
region, including the relocated footer; short boxes still exercise raw
8-bit pixel preservation.

The text/display overlay grows from 6,087 to 6,395 bytes. Resident ends remain
`$f66b` (playable) and `$f730` (full runtime suite). All overlays fit 8 KB and
the builds retain the base 1 MB NEX target. Before-change sources and frames
are under ignored `build/wrap-before/`.

The complete runtime run finishes with 53 checks, parser with 38 and renderer
with 16, all with zero failures. All 20 original game-file hashes match the
baseline. Evidence is retained as `runtime-word-wrap-tests.txt`,
`runtime-word-wrap-feedback.txt`, `parser-word-wrap-tests.txt` and
`render-word-wrap-tests.txt` under `build/`.

## Follow/wander and sound planning — 24 October 2026

The reusable runtime now implements follow.ego and wander, with direction
decisions in code page 95. Actor records remain 32 bytes. Completion flags,
step timing, stalled-move detection, collision/border handling and return to
player control are exercised by authored fixtures and original SQ1 resources.

Fresh CSpect runs report:

```text
follow-tests.txt:  SUMMARY checks=49 failures=0
motion-tests.txt:  SUMMARY checks=56 failures=0
room-tests.txt:    SUMMARY checks=28 failures=0
runtime-tests.txt: SUMMARY checks=53 failures=0
parser-tests.txt:  SUMMARY checks=38 failures=0
render-tests.txt:  SUMMARY checks=16 failures=0
```

The follow suite covers all eight directions, exact/inside arrival thresholds,
baseline centres, moving ego, detours, zero-step bounds, step.time, stopped
and erased actors, wander counters, border events, control cancellation and
VM dispatch. Five additional checks execute the unmodified LOGIC 110 droid
module with staged game flags/coordinates: initialization, starting follow,
movement/composition with VIEW 46, switching to wander, and continued
movement/composition. This is a focused integration fixture, not a playthrough
to the planet. Existing original-room travel through 2 -> 3 -> 2 -> 1 -> 2
also passes.

Full runtime checks include the stack guard, resource lifetimes, five menu
CRCs, six dialogue restoration CRCs and eight word-wrap reference CRCs. The
complete 1,599-scan intro retains `1100` story/graphics CRCs. Opening-room
performance remains 163 idle / 220 walking frames for 16 updates, matching
the preceding build; these counters do not benchmark a crowd of followers.

The new motion overlay uses 725 bytes. Page 53 uses 4,651 bytes and page 54
uses 7,030. Playable resident end is `$f6ac`, full runtime `$f771`, and motion
suite `$f54b`. All build checks pass for 8 KB overlays, reserved IM2/stack
space and the base 1 MB target. The largest new test overlay is 6,834 bytes.

The sound resource survey and [implementation plan](sound-plan.md) are saved.
No audio driver or audio slowdown claim is included in this milestone.
All 20 original game files retain their baseline SHA-256 hashes.
Evidence copies are `follow-automatic-tests.txt`, `motion-automatic-tests.txt`,
`room-automatic-tests.txt`, `runtime-automatic-tests.txt`,
`runtime-automatic-feedback.txt`, `parser-automatic-tests.txt` and
`render-automatic-tests.txt` under ignored `build/`. Previous sources/binary
and the parser baseline are in `build/motion-before/`.

## Inventory and item inspection (24 October 2026)

Fresh CSpect runs after inventory integration report:

```text
inventory-tests.txt:      SUMMARY checks=50 failures=0
inventory-game-tests.txt: SUMMARY checks=9 failures=0
runtime-tests.txt:        SUMMARY checks=53 failures=0
parser-tests.txt:         SUMMARY checks=38 failures=0
render-tests.txt:         SUMMARY checks=16 failures=0
motion-tests.txt:         SUMMARY checks=56 failures=0
follow-tests.txt:         SUMMARY checks=49 failures=0
room-tests.txt:           SUMMARY checks=28 failures=0
```

The authored inventory suite includes encrypted/plain OBJECT loading, 0/43/256
objects, name offsets/termination, name substitution, selection and paging,
malformed VIEW resources, exact 511-character description bounds, mirroring,
transparency, modal continuation, resource-pin release and stack/bank guards.
Full 80 KB framebuffer CRCs verify restoration, including non-AGI-colour pixels
in the untouched margins. Independent, hand-laid-out reference frames verify
description wrapping/substitution and the final page's remaining text.

Nine original-game checks boot SQ1 to room 2, invoke inventory and selectable
inspection through its controller conditions, inspect the keycard, execute the
survival-kit contents branch, and display the live buckazoid count. Possessions
are deliberately staged in RAM for these branch tests; assets stay unchanged.
Generated keycard and buckazoid screenshots were visually checked. Their
descriptions and cel images are intact, with words kept together.

The full intro still completes 1,599 scans with matching `1100` story/graphics
CRCs. Five menu, six dialogue and eight word-wrap CRC checks pass after moving
raw text scratch to banked RAM. Normal dialogue close remains 4 emulated
frames. Full inventory restoration takes 36-37 frames (about 0.74 seconds at
50 Hz); it restores the entire viewport rather than a small dialogue region.
Opening-room performance remains 163 idle / 220 walking frames for 16 updates.
The controlled input-on/off/repeat comparison is 181 / 181 / 181 frames;
settled death is 111 frames. These are emulated 28 MHz measurements; diagnostic
launches use `-freerun` to remove host pacing.

Production resident end is `$f5c7`, full-runtime diagnostic `$f68c`, inventory
diagnostic `$f464`, and motion diagnostic `$f466`. Inventory/motion code shares
page 95 (4,542 bytes); text/display page 55 is 7,328 bytes. The inventory test
overlays use 7,195 and 2,097 bytes. Every build passes the 8 KB overlay, base
1 MB and reserved IM2/stack checks. The resident relocation saved 229 net
bytes despite the new UI call gates and state.

The compiler's `-SO3` output dropped the original byte from constant-index
array XOR assignments in the first OBJECT loader revision. The loader now
uses a small volatile-read decrypt helper; the encrypted name/location and
original-game tests cover the corrected emitted Z80 code.

All 20 supplied game files retain their baseline SHA-256 hashes. Production
`runtime.nex` SHA-256 is
`8533843C5A49F1C369DE9D34F4038A27F1281673350EDC5D891B660DC83F1193`.
Evidence copies are `inventory-verified-tests.txt`,
`inventory-game-verified-tests.txt`, `runtime-inventory-tests.txt`,
`runtime-inventory-feedback.txt`, `parser-inventory-tests.txt`,
`render-inventory-tests.txt`, `motion-inventory-tests.txt`,
`follow-inventory-tests.txt` and `room-inventory-tests.txt` under ignored
`build/`. Previous runtime sources/binary are in `build/inventory-before/`.


## AY playback milestone - 25 October 2026

The sound suites pass 41 authored/stress checks and two all-resource checks.
All 50 readable original sounds reach the independently calculated completion
boundary and set their flags; note values are also read back from both CSpect AY
chips. Real interrupts preserve the NextReg selector, data/code/pixel windows,
cache copying, picture decoding and modal/menu restoration. A fixed movement
and input comparison yields identical actor state and framebuffer with playback
on/off. The user confirmed smooth title/menu music at capped 28 MHz.

Regression results: runtime 53, parser 38, rendering 16, motion 56, follow/wander
49, room transitions 28, inventory 50 and original inventory branches 9 checks,
all with zero failures. Full intro still completes 1,599 scans with matching
`1100` story/graphics CRCs. Average original-resource audio cost is 0.93% of the
CPU, conservatively under 1.54%; the artificial dense-note case averages 3.03%.
See [sound notes](sound-plan.md) for the measurement limits and synthesis limits.

Production resident end `$f490`, runtime diagnostics `$f555`, sound diagnostics
`$f3f2`, motion `$f32f`, inventory `$f32d`; all remain below IM2 at `$f800`.
Pages 53/56 use 5,770/5,117 bytes; no new RAM beyond the base 1 MB is required.
All 20 original game files retain their baseline SHA-256 hashes. Playable NEX
SHA-256: `6ADFF6E00679FA562C7ECCD21C5D19167D5FB9FA75D999BD7F2510E33F947058`.
Milestone logs are preserved under ignored `build/*-audio.txt`, alongside
`build/sound-before/` containing the prior executable and affected source files.

The standalone resource-cache regression also passes all 462 readable game
resources and 33 cache/error checks after splitting cold loading code.
Evidence: `build/resource-tests-audio.txt`.

## Save-dialog completion - 25 October 2026

Confirmed saves now close the slot dialog after the write has been verified,
without the extra Enter previously required by the completion notice. The
33-check save suite passes: confirmation suspends the VM, the second Enter
restores the exact pre-dialog framebuffer, and the suspended script resumes
without further input. Error dialogs remain visible and dismiss cleanly.
The screen baseline is captured immediately before opening the dialog, after
any prior scan's score/status update.

Original SQ1, KQ1 and PQ1 startup, movement, parser and save/restore controller
checks pass with the two-Enter save flow. Both player saves and all 92 supplied
game files retain their pre-change checksums. Tests write only to the isolated
`build/save-sd` folder. Evidence: `build/save-tests-close.txt`,
`build/compatibility-tests-close.txt` and `build/save-close-validation.json`.

Production resident end remains `$f695`; page 95 uses 6,081 bytes. The build
passes the base 1 MB, overlay and IM2/stack limits. Production NEX SHA-256:
`BA9E6DC4AAD26A954A5AD9CD02120F64317A758F7B279F18E3FBB08727B4146C`.


## Keyboard, labelled saves and SQ1 script waits - 25 October 2026

All 40 keyboard/save checks pass. They cover every Extended Mode digit,
classic modifier fallback, release ordering, arrows/Graph/Break/Delete,
editable header labels, arrow slot selection and exact screen restoration.
The original SQ1, KQ1 and PQ1 controller paths save and restore successfully.
An exported SQ1 slot-list framebuffer was rendered and visually checked for
vertical labels, the selected-row highlight and readable instructions.

The two SQ1 faults were reproduced with original game bytecode, staging its
normal flags/counters and scan offsets in RAM rather than replaying the whole
adventure. LOGIC 10's pod-monitor loop remains running without a key, then a
letter key returns to graphics and command input with no stale pending key.
The LOGIC 0 countdown tail at offsets 1324–1327 now executes shake.screen and
enters room 77, where the original explosion dialogue opens. Assets are unchanged.

Additional regressions pass: runtime 53, parser 38, rendering 16, sound 41 and
all-original-sound checks 2. The complete 1,599-scan intro still gives matching
`1100` graphics/background CRCs. Interrupt selector/mapping preservation and
stack guards pass. All 92 supplied game files retain their recorded hashes.
Tests use `build/save-sd` for saves; player save files are not written by tests.

Evidence is `build/keyboard-*-tests.txt`, `build/keyboard-runtime-feedback.txt`,
`build/keyboard-validation.json` and `build/sq1-save-list.png`.
Production resident end is `$f562`, full-runtime diagnostics `$f791`.
Pages 52/53/55/92/95 use 3,840 / 6,429 / 7,625 / 6,927 / 6,700 bytes.
Cold startup, full-picture presentation, console font and clock setup use
existing code banks; keyboard sampling and sound remain resident. All builds
pass base 1 MB, 8 KB overlay and IM2/stack bounds. The last four bytes added to
page 95 after these tests only change the cancel hint to `ESC/Break`.

CSpect BASIC mode was confirmed by the user to provide arrow movement, and
`-esc` allows Escape to operate the AGI menu. The host equivalent of the Next's
Extended Mode function-key layer remains under investigation. Physical Next
keyboard and storage validation are still outstanding.

### CSpect keyboard bridge follow-up

The launcher stages CSpect locally and builds `tools/cspect/ZXQuestKeyboard.cs`.
CSpect loads the plugin from memory, so its log uses the application base
folder. KeyPress registration uses the string Tab binding and an explicit
callback ID, rather than an OpenTK enum through the port-registration overload.
Nine host contract checks pass, including registration, one-shot consumption,
reset and unrelated-input handling. The live log records Tab queued/read pairs.
The user's latest response, "Same game opened", is interpreted as Save Game
opening after Tab then 5; other function keys have not been individually
confirmed on the host keyboard.

The latest core builds end at F592 (production) and F7C1 (runtime diagnostic).
Earlier preserved keyboard/save results contain 43 passing checks. Subsequent
runtime/save reruns were interrupted before fresh completion results were
available; those reruns are not additional passes. Physical Next checks remain
outstanding.

Tab-5, arrows and Escape were explicitly confirmed by the user. Tab-7 still
produced a literal digit. The decoder now accepts an already decoded ASCII
digit while the Extend prefix is armed, even if the later raw matrix reads
show release. A regression covers all ten digits in this condition. Production
and save-test builds pass memory bounds (F5BE/F608); the added regression has
been compiled but not yet executed. Live Tab-7 verification remains pending.

The decoded-digit fallback introduced in the preceding follow-up was reported
to flicker/reset at startup. It has been reverted, along with its new test.
The rebuilt production resident end is again F592. The failing binary is kept
as build/keyboard-flicker-regression.nex for diagnosis. Tab-7 remains unresolved;
use the game menu for Restore until its host shortcut is fixed.
Root cause of startup reset: Windows PowerShell 5 did not sort the bank hashtables by page, omitting linker origins. Converting bank entries to PSCustomObject before Sort-Object fixes both linker settings and validation. The corrected build reports all nine bank pages. Keyboard fallback remains reverted pending separate testing.

User sequence confirmation: typing 5678 then Backspace leaves 567; Tab-7 opens
Restore and Escape closes it immediately. The queue trace records 53,54,55,56,
12,134,7,32 in order for this sequence, with no extra zero event. This confirms
correct behaviour in this run, not a proven fix for the earlier intermittent
report. Menu navigation flicker remains outstanding. Trace preserved in
build/keyboard-user-sequence-confirmed.log.

Help-screen follow-up: text.screen now consumes the preceding key and clears
pending key state. It invalidates the parser display cache without altering
the command line, so a text/graphics round trip within one VM scan redraws the
status bar. Production builds with all nine bank origins and resident end
F5BB. Live Help and audio checks remain pending; no fresh regression suite
result is claimed for this change. SQ1 reopened at 28 MHz with sound enabled.

Help dismissal now returns after delivering a key to text mode, without also
setting its bound controller. Save/Restore progress is centred at row 12.
Runtime key-label translation and Tab-I/Edit inventory are built; Tab-Q/J
are reserved pending Quit/joystick host support. Production resident F6E9,
page 95 7221 bytes, all nine overlay bounds pass. Live behaviour and layout
verification for this revision remain pending; no new suite pass is claimed.

## Game controls and UI follow-up — 25 October 2026

Production `runtime.nex` builds with resident end F711 (239 bytes below IM2);
all overlays fit, with at least 905 free bytes. New `-TestControls` target
passes 36 checks in uncapped CSpect: Quit confirmation/bypass/shutdown state,
Restart confirmation/automatic flag/reset state, paused audio and clocks,
function-key dispatch, joystick directions/release/buttons, original SQ1
Help/Inventory/Sound/Quit/Joystick/Restart/Pause controller paths, and font
apostrophe mapping. Test flow dismisses authored alarm messages before the
next shortcut; menu tests enter through original game logic.

UI suite passes 14 checks. Original landing dialogue was captured and visually
checked with an apostrophe in You're. Save and Restore list captures use
dash/description rows without visible numbers; before/after Down captures
differ only in ten pixels inside the old and new marker cells.

CSpect does not echo writes to NextReg 05's joystick routing field in the
probe. Digital input decoding and selection were exercised deterministically;
physical connector routing and hardware reboot on Quit remain unverified.
Quit requests Next hard reset only for machine ID 10; the emulator receives
a stopped screen instead. Production reset/exit was not invoked in the
diagnostic. Existing player saves were not modified and the user's running
game was left open.

## Kerona playthrough reports — 26 October 2026

The new `tools/build_banked.ps1 -TestKerona` diagnostic reproduced 12 failing
checks before the fix, including the exact `STOP 11 LOGIC 27 OFFSET 82 OPCODE
25` report. All 17 checks pass after the actor changes:

* Original LOGIC 111 reaches its planet-detail and navigation-status displays.
  The Layer 2 capture was visually checked for the complete profile text.
* Original LOGIC 110 completes landing and enters both follow and wander modes.
  In 120 movement ticks, pursuit moved on 31 ticks before reaching its target;
  wandering moved on 112 ticks and selected several directions. Random-motion
  rules were not altered: repeated animation completion had reset its timer.
* LOGIC 26's successful put-rock branch restarts the stopped door actor, reaches
  cel 5, and stops updating as the script requests. Its open-door capture was
  visually checked.
* LOGIC 27 accepts its staged Y=172 position and runs 80 waterfall cycles without
  an interpreter error. `draw` retains the existing bounded placement search.

These are staged original-script regressions, not a complete SQ1 playthrough.
The baseline is retained in `build/kerona-baseline.txt`; results and captures
are `build/kerona-tests.txt` and `build/kerona-*.l2`/`.png`.
Existing collision/movement (60 checks), follow/wander (49 checks), and
original room traversal (28 checks) suites also pass: 154 checks in total.
Production `build/runtime.nex` builds with resident end F711
(239 bytes below IM2). Page 54 uses 7049 bytes; every overlay fits.
No game assets, player saves, or save-state layouts were changed.

## Rock-bridge diagonal wandering — 26 October 2026

A 1200-cycle original-room-19 trace reproduced extended edge sliding: diagonal
directions continued moving horizontally after their vertical component hit
the horizon or bottom edge. The baseline had 203 consecutive cycles at the
same Y coordinate, although the droid also traversed Y=86..167 elsewhere.
`build/bridge-baseline.txt` preserves this trace.

Wander now requests a fresh direction after a boundary-clipped step, including
a partially successful diagonal. This is an intentional movement adaptation,
not a change to AGI's direction codes or random ranges. See automatic-motion.md.
The motion suite passes 60 checks, follow/wander 54, and room traversal 28.
New cases cover a true two-axis diagonal, bottom/horizon/side clipping, and
the next step consuming a new random choice rather than waiting out the counter.
The production image builds with resident end F711 and page 54 at 7133 bytes;
all overlay bounds pass. Player saves and their format remain unchanged.
The extended Kerona suite passes 20 checks (162 checks total across the four
suites). Its 1200-cycle bridge run contains 354 actual two-axis movements and
covers Y=86..167. The longest constant-Y run drops from 203 to 150 ticks;
ordinary horizontal wander choices can still legitimately last many ticks.
The test stages original scripts and actor state; it does not restore or
modify the player's slot 9.

## Crush-window waiting and replacement droid — 26 October 2026

The extended Kerona suite passes 26 checks. Two staged successful rock kills
execute the original room-19 action branch and original main-LOGIC arrival
timer: with flag 96 clear, no replacement appears; with it set, a replacement
lands after the timer expires. Both kills set destruction flags 161/165 and
clear active flags 97/101. The crush does not cancel flag 96. Read-only inspection
of the player's slot 9 found 96 and 97 both set; original LOGIC 15 can arm this
timer on returning from below before destruction flag 161 has been set.
This confirms a supplied-game script quirk, not a failed crush callback.

The 1200-update bridge sample enters the exact crush zone six times, first on
update 8, with a longest absence of 302 updates. First-60 room-script, actor and
render work totals 780 emulated 50Hz frames at 28 MHz: 260 ms per update. This
excludes main LOGIC 0, parser, game pacing and diagnostic log IO, so it is a
partial CPU benchmark, not a measurement of complete gameplay throughput.
At that rate alone, 302 updates cost approximately 79 seconds. No additional
gameplay changes or production rebuild were made for this investigation.

## Skimmer print.at implementation — 26 October 2026

Reported STOP 10, LOGIC 33, PC 189, opcode 97 is the original
`print.at(9,2,3,36)` settlement-on-horizon message. Both print.at and
print.at.v now use the existing modal renderer with bounded explicit placement
and width. Only the message number is indirect in print.at.v. Ordinary
centered print remains unchanged.

The CSpect UI suite passes all 23 checks, including explicit placement,
variable-message semantics, clipping, default width and exact framebuffer
restoration after dismissal. The original LOGIC 33 reaches the reported
message and continues for 80 updates after dismissal without a STOP.
The captured ui-skimmer.png was visually checked: the complete settlement
message appears in a bordered box above the skimmer. This is a targeted
regression check, not a complete skimmer playthrough.

The production build passes all bank bounds: resident end F711 leaves
239 bytes below IM2; page 55 is the tightest overlay with 503 bytes free.
The three SD-package NEX files were refreshed from build/runtime.nex,
with packaged saves retained and the SHA256 manifest regenerated.

## Ulence Flats gambling — 27 October 2026

Restore previously rewound the serialized random generator. Successful restore
now advances the live generator mixed with the frame clock, retaining the old
file layout. Invalid restores leave that generator unchanged. The AGI random
opcode rotates its 16-bit generator output before range reduction: using the
lowest bits directly made random(0,3) repeat a four-draw cycle. No game scripts,
odds tables, prize amounts, or original assets were edited.

Actor dirty rectangles now repaint intersecting permanent nonblank text cells,
including partially intersected glyphs. Blank padding is not repainted over
newly drawn payout symbols. Full-screen text reconstruction is unchanged.
F4/F6/F8 labels become E4/E6/E8, preserving the Extended Mode key mappings and
fitting the original LOGIC 75 betting instructions within 40 columns. Break,
Edit and all keyboard decoding remain unchanged.

The final UI suite passes 31 checks: all 16 adjacent pairs occur in a seeded
256-draw four-symbol test; the original gambling scene runs 220 payout-animation
cycles with intact text; payout symbols match the composed picture; partial
glyph overlaps remain intact. ui-gambling-wins.png was visually checked for
complete symbols, Wins labels and the single-line betting instructions.
The harness now starts its frame clock: the revised random sequence can reach
a skimmer screen shake that correctly waits for clock interrupts.

The isolated save suite passes 46 checks, including different 24-symbol futures
after two restores of the same snapshot and unchanged random state after a
failed restore. The SQ1/KQ1/PQ1 startup compatibility suite reports zero failures.
These are targeted regressions, not a full gambling playthrough or hardware run.
The production build fits: resident end F711, 239 bytes below IM2; page 92 uses
7299 bytes with 893 free; page 55 retains 503 bytes free. The SD-package binaries
and hash manifest were refreshed without changing any packaged saves or assets.

## Completed SQ1 hardware route and remaining reports — 27 October 2026

The user completed SQ1 on a physical ZX Spectrum Next with 184/202 points.
This validates an end-to-end route, not every optional interaction. KQ1 and PQ1
remain untested by the user; earlier automated startup checks are not full-game
validation. Hardware reports Firmware 1.21 and Core v3.01.0.

Open reports for the next implementation pass:

* Slot-machine balance text such as "You have $189" extends black padding
  beyond the display, leaving one cell after a subsequent redraw. Reproduce
  with one-, two- and three-digit balances and check pixels outside the panel;
  do not assume the previous 220-cycle payout-label test covered this path.
* Star Generator LOGIC 65 lacks visible key-selection feedback, and initial
  dots survive the scrolling detonation display. The previous text-preservation
  repair repaints nonblank cells after actors; this is a likely contributor
  to stale dots. The correct fix needs text/sprite draw ordering, not a blanket
  rule placing all text permanently above sprites. Key feedback uses VIEW 244;
  the scrolling display uses VIEW 246. Their original script paths need tests.
* Some commands and the input prompt survive when input is disabled for a
  cutscene. rt_parser_command(0x77) currently only clears input_enabled, and
  prompt() exits immediately when input is disabled. Erasing an already drawn
  prompt must preserve the parsed command long enough for its current scan.
* Break does nothing and Edit opens Menu on Core 3.01.0. Startup only suppresses
  synthesized matrix chords on core 3.1.5 or later. The older path receives
  Edit as Caps Shift+1, currently mapped to Menu. Core-aware fallback and a
  physical-key diagnostic were identified as possible follow-up work. After
  updating firmware and core, the user confirmed Break opens Menu and Edit
  opens Inventory. Exact updated version numbers were not supplied. The agreed
  next-update mapping below supersedes the earlier Edit tap/hold proposal.

Read-only inspection of original LOGIC 114 confirms deliberately weighted slot
outcomes. In its normal random path, random(1,100) values 1 and 2 select three
skulls: approximately 2% per spin. The other scripted branches give diamonds
1%, eyes 2%, three cherries 4%, two cherries 7%, one cherry 16%, and the losing
branch 68%. The separate variable-134 override is excluded from these odds.
The user seeing three skulls once does not establish a distribution defect.
No odds, game scripts, runtime code or executable were changed in this audit.

## Agreed controls for the next update — 27 October 2026

Record only; implementation and a new build are deferred to the next update.

* Break: Menu / Cancel, with gameplay paused while the menu is open.
* INV VIDEO: Inventory.
* Hold Edit and press 1–9, 0: Sierra F1–F10.
* Edit + Q: Quit; Edit + J: Joystick selection.
* Use E1–E9 and E0 consistently in menus, Help and game-message replacements;
  use E-Q / E-J for the corresponding letter shortcuts. Explain explicitly:
  "E5 means hold EDIT and press 5." E now denotes Edit, not Extended Mode.
* Edit alone is solely a modifier; no tap-for-Inventory or timed hold detection.

Update runtime decoding, regression tests, README, keyboard/save documentation,
Help/menu labels and SD-package instructions together. The current executable
and its current-use instructions retain their existing mappings until then.

Additional user report: after restarting the game, Break no longer opened the
menu, despite working earlier on the updated firmware/core. Cause unconfirmed.
Add a cold-start versus in-game-restart regression for Break, checking again
once gameplay resumes. Inspect keyboard register configuration, decoder state,
input queues and game menu/controller bindings across restart. This report is
recorded for the next update; no runtime change or new build was made.

Additional agreed controls work: simultaneous arrow keys should move Roger
diagonally in all four directions. Cover staggered key presses/releases,
cardinal/diagonal transitions and opposing pairs, while retaining menu and
save-slot navigation. This is a checklist addition only, not a runtime change.

## Next controls and restart fix completed - 9 November 2026

Implemented INV VIDEO for Inventory, held Edit + 1-9/0 for Sierra F1-F10,
Edit + Q/J for Quit/Joystick, and E1-E0 / E-Q / E-J labels. Edit alone has
no action. Extended Mode remains a compatibility alias. Help now explains
the notation, names BREAK, and describes two-arrow diagonal movement.

Dedicated arrows support all eight directions. Adding a second arrow changes
heading immediately; staggered releases retain the selected direction until
all arrows are released. Pressing that direction again stops walking. Opposing
axes cancel. Cardinal menu and save-slot navigation remains supported.

The restart defect was lost menu definitions: rt_init cleared them, while SQ1
LOGIC 0 deliberately skips menu construction when restart flag 6 is set.
Restart now preserves key/menu definitions through scratch page 91 and enables
items before the original scripts apply their restrictions. Input decoding and
queued events reset, waiting for held keys to be released. No save format changed.
The decoder moved to bank 52 through a scoped resident gate; IRQ bank mappings
are restored on return, with decoder state resident.

Validation: CSpect controls suite 66 checks, save/restore suite 46 checks, and
UI suite 31 checks, all with zero failures. Compatibility suite reported zero
failures for SQ1, KQ1 and PQ1. Controls coverage includes all ten Edit shortcuts,
modifier release, all arrow directions and staggered transitions, and two actual
SQ1 restarts followed by working Break/menu, Save and Inventory. Inspected the
rendered Help and Action menu: headings remain centred, instructions fit, and
E4 / INV shortcuts align at the right edge.

Production resident end is $F5B4, leaving 588 bytes before reserved IM2/stack.
Page 52 has 549 bytes free; page 55 is tightest at 503 bytes free. All production
banks fit. Refreshed SQ1/KQ1/PQ1 ZXQUEST.NEX copies in build/sd-card/ZXQUEST and
README/SHA256 manifest; hash-verified that all 50 other packaged files remained
unchanged. NEX SHA256:
496B6E039BF7A94214807974B30EF00106272E7E3C69022448C14B800C69FBCB.

Physical Next verification of the new Edit combinations, INV VIDEO, diagonal
chords and Break after restart remains pending. Emulator tests exercise raw key
samples and interpreter behaviour; they do not establish physical keyboard rollover.


## Display fixes, performance and control follow-ups - 9 November 2026

Completed the requested display/performance pass, plus description-only
Save/Restore confirmation panels and removal of the Graph Inventory alias.
INV VIDEO remains Inventory. Original game files were not edited.

Display commands now respect pending actor draw order. Per-cell pixel-pair
visibility keeps subsequent sprites above earlier text without losing labels
elsewhere in a dirty rectangle. The original Star Generator keypad highlights
pressed buttons (it does not print the selected digits); its original 6858
sequence and scrolling detonation message execute successfully. White dots
remain erased after the scroll. prevent.input clears the editable command and
prompt while preserving parsed words/said eligibility for the current scan.

The original gambling scripts were exercised with one-, two- and three-digit
balances after win/loss settlement and through the return to payout labels.
Pixels beyond the display stay unchanged. Inspected the $189 result and the
following payout display, as well as the unnumbered save/restore confirmations.
The user's physical SD-card Gambling save (probably slot 6) was not mounted;
this is original-script regression evidence, not a replay of that exact save.
Recheck that hardware sequence before declaring the reported overflow closed.

Optimisations: generation-keyed cel metadata and successful RLE-validation
cache; sparse occupied-actor scans; shared VIEW/compositor code bank; separate
vertical dirty bands with control-priority dependencies; empty-text-row skip.
Malformed/truncated RLE still fails before painting. Renderer output matches
full reconstruction and preserves pixels between separated dirty bands.

Final suites: display/cache 17, UI 30, save 46, parser 38, renderer 17 checks:
148 checks, zero failures. The UI suite includes 220 original gambling updates.
The save suite verifies partial-text metadata and Graph emitting no inventory
event; the display suite reconstructs visibility metadata absent in older saves. An intermediate save run reused prior test
slots and failed five empty-slot/generation assumptions; clearing only
build/save-sd/saves and rerunning resolved all five. No player saves were touched.
The baseline glyph reference test was updated for the already-supported AGI
apostrophe/caret substitutions rather than comparing those to the Spectrum ROM.

Matched 28 MHz corridor CPU benchmark: 16 walking scans 226 -> 188 frames
(1.20x throughput), idle 160 -> 116. Further original generator/gambling stage
profiles are recorded in performance.md without unmatched speedup claims.
Production remains a base-1-MB NEX with resident end $f600 / 512 bytes free.
Page 55 is tightest at 424 free; current bank totals are in runtime.md.

Refreshed identical SQ1, KQ1 and PQ1 NEX copies plus package README/manifest.
Verified all 50 other package files, including saves, configs and game data,
remained unchanged. Runtime SHA256:
4C38510E2D4DDD8A56F9A4AD21338F59905AF6ADC227D493B60BA58DD9FBFDD0.

The final compatibility suite also completed with zero failures for KQ1, PQ1
and SQ1 startup, movement, LOOK, save/restore and menu-controller paths, plus
the SQ1 pod-screen dismissal and ship explosion. This is not a full-game
KQ1/PQ1 playthrough. Completed diagnostic emulators were closed.
