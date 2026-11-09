# Sound playback

Implemented 25 October 2026 for CSpect at capped 28 MHz, within the base 1 MB
layout. Original AGI SOUND resources remain separate and unchanged. The user
confirmed smooth title music, including with the Shift+1 menu open. Physical
Next audio and wider-game compatibility remain to be checked.

## Playback and compatibility

AY1 A/B/C play the three tone streams. AY2 supplies independent noise (or a
square-wave approximation for periodic noise). The Next's 1.75 MHz PSG enable
is documented in the [official FPGA clock divider](https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/blob/master/cores/zxnext/src/zxnext_top_issue2.vhd#L1101).
Tone periods are rounded from the original SN76489 divider using
`period = round(divisor * 35000 / 35795)`. AGI attenuation maps approximately
from 2 dB steps to AY volume levels. Frequency zero and attenuation 15 are rests.
No waveform samples are mixed by the CPU.

The resident IM2 player advances six sound ticks for every five 50 Hz interrupts.
The first note is primed immediately; a duration of N lasts N sound ticks.
Each tick reads at most one event per channel; a frame processes at most eight
events. Notes of one tick have frame-quantized timing, including the double-tick
frame. Notes continue through picture decode, actor drawing, menus and dialogues.
Only changed AY registers are written, with a small assembly leaf for port I/O.

A completion event crosses from the interrupt to foreground code; the IRQ never
calls the VM or changes its flags. Natural endings, stop.sound, replacement,
room transitions and reset silence playback. Stop/replacement complete the old
sound's flag before the new flag is reset. Flag 9 disabled at start completes
immediately without loading; clearing it during playback silences at the next
interrupt. Fatal exits in the playable main also silence audio. This completion
behavior agrees with the [ScummVM sound manager](https://github.com/scummvm/scummvm/blob/master/engines/agi/sound.cpp);
its implementation was consulted for semantics, not copied.

Zero-duration records are rejected explicitly: original AGI v2 timing has an
unhelpful stall behavior for them, and none of the supplied sounds contain one.
Missing/truncated records, overlapping streams, bad offsets, absent terminators
and payloads larger than 6,656 bytes return an explicit error. Four empty streams
complete immediately. SOUND 0's pre-existing missing VOL.15 remains an error.
All 50 readable supplied sounds are supported, including the largest 5,896-byte
resource. Another game's larger sound requires a future bank strategy.

The timbre is approximate: AY white noise has a five-bit period, so slower noise
rates saturate; periodic SN noise is approximated with a square wave at its
15-stage repetition frequency. Tone-3-coupled noise follows that tone's divider.
The Sierra per-tick dissolve envelope is not yet reproduced; volumes follow the
resource events. This is recognisable AY playback, not bit-exact PCjr emulation.

## Memory and interrupt contract

Cold resource opening/loading and cache allocation/eviction moved to overlay 53.
Hot resource reads/copies, token lookup, pin release and bank transfers remain
resident. Sound preparation and foreground completion handling share overlay 56.
The playable resident image ends at `$f490`, leaving 880 bytes before IM2;
overlays 53 and 56 use 5,770 and 5,117 bytes. All build memory guards pass.

A validated private sound copy occupies page 91 at `$6600-$7fff`, after actor
scratch/baselines. Cache eviction or cache reset cannot invalidate playing notes.
Preparation uses resident staging, validates all streams before converting, and
never modifies the cached original. Preparation is foreground work and can delay
a new sound's start; it does not run with interrupts disabled.

The IRQ saves both MMU3 and the selected NextReg port register, maps page 91,
then restores both. Restoring the selector protects an interrupted `OUT $243b`
/ `IN or OUT $253b` pair. MMU2 and its pixel-window cache are never touched.
The existing IM2 wrapper preserves CPU registers. Short start/stop critical
sections preserve the prior interrupt-enable state. Resource APIs remain
foreground-only; no filesystem, code overlay, allocation or cache call is
permitted in the IRQ. AY1 and AY2 ports are owned by this backend.

## Measurements and tests

The original target was at most 2% additional average CPU, measured against the
clock/keyboard-only interrupt, with a separate worst-case record. All results
below use emulated 28 MHz CPU time; diagnostics use `-freerun -sound` only to
remove host pacing and audible output. These switches do not remove AY register
processing or change the emulated 28 MHz clock.

| Test | Result |
| --- | --- |
| All 50 original sounds, 8,252 scheduled frames | 77 extra CPU frames: **0.93%** average |
| Same, allowing one measurement frame of uncertainty per resource | **under 1.54%** |
| Artificial four-voice changes every tick, 4,000 scheduled frames | 121 extra frames: **3.03%** average |
| Maximum observed stress IRQ body, including raster sampling | 17 of 312 lines: **5.45% of a frame** |
| Fixed movement + two input events, 16 scans | 57 clock-only / 58 sound-enabled frames; identical actor state and full framebuffer |

The average target passes for the actual SQ1 resource set; it is not a worst-case
2% guarantee for arbitrarily dense sounds. The stress figure includes every
voice changing every tick and both ticks on the fifth frame. The raster figure
covers the sound body and sampling overhead, not the common clock/keyboard ISR
prologue. Frame quantization limits precision. The original room-2 harness still
measures 220 walking frames for 16 updates and 4 frames to close a normal dialogue.

41 authored/stress checks and two whole-resource checks pass. They cover pitch,
volume, rests, both AY chips' actual register readback, end flags, replacement,
stop/mute/reset, malformed inputs, IRQ enable state, bank/selector interleaving,
cache resets and copies, picture decode, full modal/menu restoration, timing,
fixed movement/input equivalence and stack guards. All 50 final sound ticks
match independent host duration totals. Existing runtime, parser, rendering,
collision, follow/wander, room and inventory suites pass as well.

Diagnostics expose `sound_frames`, `sound_ticks`, `sound_writes` and
`sound_peak_events`. Test mains explicitly select deterministic manual sound
frames for older virtual-time game fixtures. Production uses the IRQ clock;
`rt_step` only polls completion. The manual/bypass switches default to zero.

```powershell
python tools/prepare_runtime_tests.py
python tools/prepare_sound_tests.py
.\build.ps1 -SoundTest
.\run.ps1 -SoundTest
# Wait for SUMMARY in both logs:
Get-Content build/sound-tests.txt
Get-Content build/sound-game-tests.txt
# Audible, capped normal play:
.\build.ps1 -Runtime
.\run.ps1 -Runtime
```

The installed CSpect `-sound` switch **disables** audio. Normal `run.ps1`
launches omit it; diagnostic launches retain it with `-freerun`. Evidence is
saved in ignored `build/*-audio.txt`; the pre-change executable/source snapshot
is `build/sound-before/`. `tools/analyse_sound.py` provides a read-only survey.
Listening to individual door, alarm, gunshot and death effects in a longer
playthrough, and checking the output on physical Next hardware, remain useful
follow-up validation.

Additional hardware references: [chip selection](https://wiki.specnext.dev/Turbo_Sound_Next_Control),
[TurboSound enable](https://wiki.specnext.dev/Peripheral_3_Setting_Register),
[AY mode](https://wiki.specnext.dev/Peripheral_2_Setting_Register),
[AGI SOUND layout](https://www.agidev.com/articles/agispec/agispecs-9.html).

The standalone resource-cache regression also passes all 462 readable game
resources and 33 cache/error checks after splitting cold loading code.
Evidence: `build/resource-tests-audio.txt`.
