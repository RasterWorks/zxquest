# Picture viewer performance — 23 October 2026

The user confirmed that N, B and P all work, but next/back were slow.
The first benchmark identified picture decoding as the largest cost.

Measurements use an IM2 frame counter in a dedicated benchmark NEX, with
CSpect at 28 MHz and a 50 Hz emulated display. Freerun removes host pacing
without changing the emulated frame durations. Each frame represents 20 ms.
This measures emulated CPU work, excluding host filesystem latency and the
small status-text update. Expect approximately one frame of quantization.

| Picture | Decode before | Decode after | Display before | Display after | Total reduction including clear |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 7.24 s | 5.34 s | 1.10 s | 0.34 s | 31.7% |
| 12 | 6.62 s | 4.92 s | 1.10 s | 0.34 s | 31.7% |
| 27 | 9.04 s | 6.84 s | 1.08 s | 0.34 s | 28.8% |
| 67 | 12.88 s | 9.36 s | 1.10 s | 0.34 s | 30.5% |

Changes:

* Flood fill writes complete horizontal spans through one mapped-memory
  operation. Spans crossing an 8 KB boundary are split explicitly.
* Line/point plotting combines the previous separate read and write lookups.
* Display copying traverses sixteen-pixel runs. Each run stays inside one
  source bank, avoiding per-pixel function calls and repeated bank changes.

No game data or rasterization rules changed. `tests/reference_picture.c` is
a frozen copy of this project's pre-optimization decoder, used only in tests
to compare complete packed picture buffers. It is not a Sierra reference and
does not establish pixel-perfect compatibility with the original game engine.

The viewer remains slow on first-time room decoding. Further work should
profile boundary scans/fills and consider assembly or decoded-room caching
within the interpreter's eventual memory allocator.

Reproduce timings:

```powershell
.\build.ps1 -Benchmark
.\run.ps1 -Benchmark
```

Read `build/benchmark.txt` after the benchmark window says complete. The
benchmark alone enables its private IM2 handler; the normal viewer's
interrupt policy is unchanged.

## Runtime startup and input follow-up

The user reported a long startup wait, missed Shift+1 presses during the title
glow, and a menu bar left behind after closing the menu. Measurements from
`-RuntimeTest` use the resident 50 Hz frame counter, with identical original
resources and script state before/after the changes:

| Work | Before | After |
| --- | ---: | ---: |
| First complete SQ1 startup/title scan | 614 frames (12.28 s) | 297 frames (5.94 s) |
| Following 24 title scans, CPU time only | 335 frames (6.70 s) | 299 frames (5.98 s) |
| Slowest of those 24 scans | 53 frames (1.06 s) | 53 frames (1.06 s) |

These are emulated CPU durations, not host stopwatch loading times. The first
scan includes script/resource initialization, picture decoding, background
capture, display and text. Expect frame quantization; animation's scripted
delays are excluded from the 24-scan CPU total. Startup CPU time fell by
about 52%; animation still contains expensive synchronous operations.

Flood fill now reads a packed row in bulk and scans the resident copy. This
removes repeated coordinate multiplication and mapped pixel function calls.
Scene capture/restore copies blocks directly between the picture and clean
background windows, splitting at 8 KB boundaries. Rectangle display reads
each source row in bulk. The same fill/rasterization decisions remain in use.

The resident interrupt now samples keys into a 31-event queue. Press/release
edges survive work that delays foreground polling, with one event per held
key and bounded overflow. The IRQ never changes MMU mappings or invokes the
VM/filesystem/renderer. This prevents dropped short presses; it does not make
a synchronous redraw instantaneous. The loading message stays visible until
the first picture is displayed.

The initial menu-restoration fix cleared the display before rebuilding the
picture and text. The user confirmed the keyboard fixes, but reported the
resulting black flash. Menus now save the visible 16-colour viewport to a
32 KB packed backing store. Closing restores only the menu bar and item rows;
navigation restores only rows exposed by a shorter menu. There is no full-screen
clear or picture reconstruction on those paths. Full-frame CRC checks cover
closing/selecting menus, alternate picture origins and text mode. Sentinel
pixels also check that uncovered areas remain untouched during navigation.
See [validation.md](validation.md) for regression results.

## Intro text lifetime

The story/credits overlap was a state-lifetime bug, rather than a slow clear.
`graphics` used to redraw persistent text from the previous text-mode screen;
`show.pic` also repainted text over the picture that was meant to replace it.
Switching video modes now discards the old text cells. Picture display only
invalidates cells in the 21 picture rows and uploads that image, preserving
the status/input margins. It no longer clears the entire display or repaints
text on every `show.pic`. The loading message sits within the first picture's
area so that upload removes it naturally.

## First-room frame-rate work — 24 October 2026

The reported roughly one update per second was reproduced with the original
opening-room scripts: 16 idle scans consumed 769 emulated frames (15.38 s),
with 574 frames in rendering, 151 in the VM and 43 in actor updates. The old
renderer erased and redrew every visible actor and uploaded their combined
rectangle whenever any actor changed.

The renderer now compares each actor against its last drawn state, expands
the changed area through overlapping actors and priority-control dependencies,
then restores and redraws that area. Priority dependencies extend through
the connected control run and its first normal pixel, not an entire column.
Snapshots fit the existing 32-byte actor records. Extra affected-actor marks
and a control-column mask share scratch page 91 with the sorting arrays.

Other measured hot paths were reduced without converting game resources:

* Cache lookups check the most recent entry first, validating the complete
  handle. Touching the most-recently-used entry no longer scans all LRU ranks.
* Sprite compositing reads/writes 32-pixel row fragments, retaining validation,
  clipping, transparency and priority rules.
* Rectangle uploads map a Layer 2 bank once per column group and increment
  destination pointers, avoiding repeated address calculations per pixel.
* Text cells map their bank once; spaces use short bulk fills.
* VM operands already present in the bounded instruction window are copied
  together. Runtime `said` checks avoid entering the parser bank when the
  standard input flags already rule out a match.

Reproduce with `build.ps1 -ParserTest` / `run.ps1 -ParserTest`, or the complete
`-RuntimeTest` suite. Read `PERF` and `RENDER` lines in `build/parser-tests.txt`.
The harness starts from original startup/name entry, runs real first-room
commands, then measures 16 idle and 16 walking scans. Original timed messages
are acknowledged through Enter; modal drawing/dismissal is excluded from
the movement measurement. The initial walking baseline stopped at one such
message, so only its completed idle run is used for a before/after comparison.

Counters report 50 Hz emulated CPU frames, with roughly one frame of
quantization per timing boundary. Script pacing and host filesystem latency
are not included. Optional stage counters are disabled during normal gameplay.
Rendering comparisons, glyph checks and the resource/VIEW/VM suites are
recorded in [validation.md](validation.md).

Final measured results:

| Work | Before | After |
| --- | ---: | ---: |
| 16 opening-room idle scans | 769 frames / 15.38 s | 178 frames / 3.56 s |
| Mean idle scan CPU cost | 961 ms | 223 ms |
| 16 walking scans (modal UI excluded) | No complete baseline | 257 frames / 5.14 s |
| Mean walking scan CPU cost | — | 321 ms |
| Type `look` and draw its original response | 111 frames / 2.22 s | 28 frames / 0.56 s |

Idle throughput improves about 4.3 times. Walking is still only about 3.1
updates per second in this measured path, below normal game speed. Of the
257 walking frames, 105 are VM work and 129 rendering; these remain the main
targets for further optimization. Picture decoding is still synchronous.

# Collision milestone measurements — 24 October 2026

The collision/room-transition build adds clean-map and object checks plus a
pre-movement geometry pass. The same first-room harness measured 195 frames
for 16 idle scans (~244 ms/scan), and 274 for 16 walking scans (~343 ms/scan).
These are emulated 50 Hz frames, excluding modal-message waits as before.
This milestone prioritizes correct movement; it is not a further speed pass.

## Dialogue dismissal and emulator pacing — 24 October 2026

The interactive launcher uses capped 28 MHz timing. Test launchers add
CSpect's `-freerun`, removing host pacing, so their visible gameplay runs
faster. All CPU measurements above use the emulated 50 Hz clock, not the host
stopwatch. The user chose to retain normal 28 MHz timing for interactive play.

Closing a print window previously called `rt_redraw`, clearing all 80 KB and
rebuilding the picture and text. It now restores the captured pixels beneath
the window. A normal dialogue saves 30,720 raw bytes with bulk copies; longer
messages pack their covered rows into the same 32 KB modal backing store.
The saved bounds include explicit newlines, automatic wrapping and clipping
at the final text row. Uncovered areas are never cleared by this path.

The same original first-room `look` sequence measured:

| Work | Before | After |
| --- | ---: | ---: |
| Interpret `look` and open its response | 29 frames / 580 ms | 33 frames / 660 ms |
| Dismiss the response, excluding resumed script execution | 30 frames / 600 ms | 4 frames / 80 ms |
| Combined open/close CPU work | 59 frames / 1.18 s | 37 frames / 740 ms |

Dismissal is about 7.5 times faster; capturing the background adds four frames
to opening. Frame-counter quantization is approximately one frame. Gameplay
movement is unchanged: 16 idle scans still take 195 frames, and walking takes
274 frames (275 in the immediate baseline). This change does not improve
walking throughput or first-time room decoding.
Extended messages use the slower packed path: the newline, bottom-clipped
and automatically wrapped fixtures close in 22, 26 and 20 frames respectively.
They still restore only covered rows and avoid the full-screen clear.

Reproduce with `-ParserTest`; `DIALOG close` isolates the close handler before
the suspended VM scan resumes. Before/after evidence is retained in ignored
`build/parser-before-dialogue.txt` and `build/parser-after-dialogue.txt`.
The full runtime suite also compares complete framebuffer CRCs for dialogue
restoration in graphics/text modes, a shifted picture, long/newline/wrapped
messages, and Enter/Escape closure.

## Faster alarm after an alien death — 24 October 2026

The user observed the alarm speeding up after an alien shot Roger and asked
whether command entry was consuming the time. The original alien encounter
in LOGIC 103 sets v130 to 1 after the alien leaves. LOGIC 0 routes v130>0 to
LOGIC 100 and returns at offset `$015d`, skipping normal room/game logic.
LOGIC 100 accepts input and eventually settles at v130=8; keyboard scanning
and the interpreter's actor animation loop continue.

`-ParserTest` now performs a controlled comparison in room 2 after the existing
walking benchmark. It holds the room/actors in place, runs 16 stationary
updates with input enabled, disables input, enables it again, then selects
the original settled-death script path. This isolates the workload difference;
it does not reproduce the user's full route or shooting animation. The
diagnostic restores the selected death-state variables after the measurement.

| 16 updates, emulated CPU time | Total frames | VM | Actors | Render | Input/status |
| --- | ---: | ---: | ---: | ---: | ---: |
| Alive, input enabled | 218 | 107 | 29 | 79 | 0 |
| Alive, input disabled | 218 | 106 | 30 | 79 | 0 |
| Alive, input enabled again | 218 | 107 | 30 | 80 | 0 |
| Settled death path, input enabled, same actors | 130 | 20 | 29 | 79 | 1 |

At 20 ms per frame, the totals are 4.36 seconds versus 2.60 seconds. The death
path reduces CPU work by about 40%, allowing about 1.68 times as many updates
per second in this controlled case. The savings are almost entirely VM work.
Stage boundaries have frame quantization and omit some outer-loop work, so
their totals need not equal the complete scan measurement.

These results rule out idle input handling as the main cause in this scene;
they do not claim typing/rendering a command is free. Keyboard sampling still
runs at 50 Hz in both states. Alarm cels advance on actor updates, so reducing
the script workload increases their wall-clock rate when normal play is CPU
limited. Script execution and rendering remain the performance targets. No
production timing or keyboard behaviour was changed for this investigation.
Evidence is retained in `build/parser-alarm-profile.txt`.

## VM fetch optimization — 24 October 2026

Sequential bytecode reads now advance a cursor in the current frame's bounded
64-byte instruction window. Calls and returns invalidate that window; branches
seek within it or force a refill. The VM keeps the current frame pointer across
ordinary instructions, refreshing it on calls and returns. This removes repeated
frame lookup, 32-bit handle comparison and window-range calculation from the
fetch path. Resource bounds, instruction budgets and diagnostic locations are
unchanged. The VM shrinks from 1,070 to 1,067 resident bytes and its code overlay
from 6,912 to 6,609 bytes.

The same original room-2 parser harness, at emulated 28 MHz, reports:

| Work | Before, frames | After, frames | Before/after VM frames |
| --- | ---: | ---: | ---: |
| 16 idle scans | 195 | 172 | 112 / 90 |
| 16 walking scans | 274 | 251 | 109 / 83 |
| Type `look` and open the response | 33 | 30 | — |
| Close the response | 4 | 4 | — |
| 16 stationary scans after walking, input enabled | 218 | 196 | 107 / 84 |
| Same actors, settled death path | 130 | 126 | 20 / 16 |

Script execution costs about 20–24% less in the normal-play comparisons.
Complete idle/walking throughput improves about 13%/9%, respectively. Walking
still costs about 314 ms per update (251 × 20 ms / 16), or 3.2 updates/second
before script pacing and host overhead. Rendering now accounts for 128 of the
251 walking frames, compared with 83 for the VM and 36 for actors. This is a
modest improvement, not normal game speed yet.

Input on/off/on after walking each takes 196 frames. The lighter death script
still allows faster animation: 196 versus 126 frames for the same 16 actor
updates. This optimization does not change the game clock, animation rules,
keyboard sampling, game scripts or CSpect's interactive 28 MHz cap.

The first-room, `look`, unknown-word and walking framebuffer exports match
their pre-change SHA-256 hashes byte for byte. Before/after logs are retained
in ignored `build/parser-before-vm.txt` and `build/parser-after-vm.txt`.
Use `-ParserTest` to repeat this measurement; timing boundaries and exclusions
remain as described above.

## Sprite composition and display copies — 24 October 2026

The sprite row compositor now compares packed visual/priority bytes directly
and advances source/destination pointers. A priority with its low nibble set
provides the same inclusive depth comparison without shifting each pixel.
Control pixels retain their high nibble and still use bounded look-down;
transparent pixels are unchanged. Complete cel validation still precedes any
drawing, including malformed or clipped cels.

Partial Layer 2 uploads use small Z80 loops to extract the chosen nibble and
write both horizontally doubled pixels. Each fragment stays within one display
bank, and its destination advances one row at a time. The staging buffer is
16 bytes instead of 160, with a one-byte count: 143 resident bytes saved.
No resource format, script, dirty-region rule or animation timing changes.

The same room-2 harness measured the following at emulated 28 MHz:

| CPU work, 16 scans unless noted | Before, frames | After, frames |
| --- | ---: | ---: |
| Idle | 172 | 163 |
| Walking | 251 | 220 |
| Walking: complete render stage | 128 | 96 |
| Walking: sprite drawing | 64 | 51 |
| Walking: display upload | 40 | 21 |
| Stationary after walking, input enabled | 196 | 181 |
| Same actors, settled death path | 126 | 111 |
| Open original `look` response | 30 | 30 |
| Close response | 4 | 4 |

Walking CPU cost falls about 12%, giving about 14% more updates per second.
The render stage costs 25% less, with display uploads almost halved. Idle
throughput improves about 6%. Walking averages 275 ms of CPU work per update
(3.6 updates/second), compared with 314 ms before. Script pacing and host I/O
are still excluded; this is not a claim of complete, full-speed gameplay.
Stage counters have frame-boundary quantization as described above.

All four first-room/command/walking framebuffer exports match the prior build
byte for byte. Before/after evidence is in ignored `build/parser-before-render.txt`
and `build/parser-after-render.txt`. Normal play remains capped at 28 MHz.



## Interrupt AY playback - 25 October 2026

Playing all 50 original SQ1 sounds costs 77 additional emulated CPU frames over
8,252 scheduled frames, approximately 0.93%. Allowing one full frame of timing
uncertainty per sound bounds this below 1.54%, meeting the 2% average target.
This measures the complete player against clock/keyboard-only timing, not merely
muted register writes. Foreground resource loading/conversion is outside this
interrupt-cost measurement.

An authored stress resource changes all four voices every sound tick. Over
4,000 scheduled frames it costs 121 additional frames (3.03%); the longest
measured body spans 17 raster lines (5.45% of a 312-line frame), including raster
sampling overhead. This is a separate worst case, not the ordinary-game average.

Fixed movement plus two input events takes 57 clock-only / 58 sound-enabled
frames for 16 scans. Both runs produce identical actor records and all 80 KB of
display pixels. The existing original room-2 benchmark measures 164 idle / 220
walking frames, controlled input on/off/repeat 181/181/180, settled death 111,
and normal dialogue close 4. This is within frame quantization of the previous
milestone. The original-game fixtures use deterministic manual audio frames;
the separate fixed-input comparison and bank-interleaving checks exercise real
interrupt playback. Normal play remains capped at 28 MHz.

See [sound notes](sound-plan.md) for the implementation contract, approximations,
reproduction commands and evidence. The user confirmed smooth title music while
opening the menu in the audible capped CSpect build.

## Post-completion optimisation priorities — 27 October 2026

SQ1 now has a user-confirmed end-to-end hardware playthrough, but remains slow.
The architecture is a native Z80 interpreter compiled from C with selected
assembly routines, executing original AGI LOGIC bytecode. It is not an x86/PC
emulator, and the original games have not been translated into native C.

The last controlled room-2 walking measurement above is 220 emulated 50 Hz
frames for 16 scans (275 ms CPU work per scan). Rendering accounts for 96 frames,
the VM approximately 83 and actors approximately 36. These are historical
measurements, not a new benchmark of the text-repair build. They identify
rendering and script execution as the first profiling targets, not input or
ordinary sound playback. Clock MHz alone cannot compare an 8088 and a banked
Z80 C implementation.

Next profiling/implementation sequence:

1. Reproduce the remaining display/input faults and establish correct output
   before optimising those paths. Preserve original assets and save compatibility.
2. Refresh stage timings at 28 MHz in the corridor, gambling scene and Star
   Generator panel. Separate resource IO, VM dispatch, actor work, composition,
   text repair, display upload and intentional game pacing.
3. Investigate cached validated cel metadata/decoded rows. agi_view_blit currently
   decodes each cel once for validation and again for painting, while render
   reacquires and selects affected views each time. Cache validity must track
   resource identity/eviction and retain malformed-resource checks.
4. Measure separate dirty regions instead of one union spanning distant actors;
   retain actor overlap and control-pixel priority dependencies. Batch bank
   mappings and copies where profiles justify it.
5. Reduce measured VM callback/bank-switch overhead, then consider assembly for
   remaining hot loops. Ahead-of-time compilation of all game scripts is a much
   larger change and is not required to begin these improvements.

Illustrative only: halving both rendering and VM costs in the old profile would
reduce 220 frames to about 130.5, or about 1.69x throughput. This is arithmetic
on a historical profile, not a promised improvement or a measured new result.


## Display correctness and gameplay optimisation - 9 November 2026

Fresh before/after measurements use the same original SQ1 room-2 workload,
16 completed scans, CSpect 28 MHz and emulated 50 Hz frame counts. Freerun
removes host pacing; it does not change the target CPU clock. Deliberate game
speed delays and modal announcements are excluded from these CPU-work totals.

| Workload | Before (frames) | After (frames) | Update throughput |
| --- | ---: | ---: | ---: |
| Opening corridor, idle | 160 | 116 | 1.38x |
| Opening corridor, walking | 226 | 188 | 1.20x |
| Controlled alive, input enabled | 183 | 144 | 1.27x |
| Controlled alive, input disabled | 183 | 144 | 1.27x |
| Same actors, settled death | 114 | 73 | 1.56x |

The walking sample's actor stage fell from 43 to 14 frames. Rendering fell
from 99 to 86; VM time stayed around 84. Sub-stage values have frame-boundary
quantisation, and do not include all loop/gate overhead. This is a corridor
benchmark, not a claim that the entire game is 20% faster. Disk loading and
intentional animation pacing are not addressed by these changes.

Changes: generation-keyed cel metadata/RLE-validation cache; empty eight-ID
actor groups skipped; VIEW/compositor direct calls in shared page 93; vertical
dirty bands instead of uploading every gap in a single large union; text
visibility row bits avoid work where no display text exists. Pixel-accurate
text occlusion fixes remain enabled in these measurements.

Additional original-script profiles from the final display suite: the Star
Generator runs 96 scrolling updates in 58 VM / 61 actor / 253 render frames;
the three gambling result/payout sequences total 48 updates in 79 VM / 31 actor
/ 11 render frames. These harnesses exclude resource initialization and do not
supply matched pre-change percentages. The first generator baseline covered
only its first 16 updates, so comparing it directly to the 96-update run would
be misleading.

Evidence: build/display-baseline-parser.txt, build/parser-tests.txt,
build/render-tests.txt and build/display-tests.txt. The retained stage profile
still identifies LOGIC execution and composition as the next speed targets.
