# Hardware installation and testing

This preview contains the interpreter only. Supply your own original DOS/EGA
AGI game resources. SQ1 and KQ1 have been completed on a real Next; PQ1's
hardware playthrough is in progress. Other AGI games are not yet validated.

## Install

1. [Download the prebuilt ZXQUEST.NEX](https://github.com/RasterWorks/zxquest/raw/refs/heads/main/dist/ZXQUEST.NEX)
   and copy it into a folder such as `ZXQUEST` on your SD card. No compilation
   or emulator is required. If you download the repository ZIP instead, use
   `dist/ZXQUEST.NEX` from it.
2. Create a subfolder for each game (for example `SQ1`, `KQ1`, `PQ1`) and copy its resources into it:
   `LOGDIR`, `PICDIR`, `VIEWDIR`, `SNDDIR`, `OBJECT`, `WORDS.TOK` and every `VOL.*`.
3. If your existing installation includes `ZXQUEST.CFG`, change its contents to `AUTO`
   or remove it to enable the picker. No configuration is required otherwise.
4. Open the folder containing `ZXQUEST.NEX` in the NextZXOS browser and launch it.
   One game loads automatically; use Up/Down and Enter when several are found.

These must be your own **DOS/EGA AGI v2** files; no games are supplied with
the interpreter. AGI v3 and SCI/VGA remakes are not supported. The executable
and its checksum/source revision are listed in the repository's
[download folder](https://github.com/RasterWorks/zxquest/tree/main/dist).
For an update, replace the NEX only and keep your game files and saves.

The same executable runs all three games. Discovery also checks `games/`
subfolders and legacy per-game folders whose CFG points to nested resources.
Saves go directly in the resource folder's `saves` subfolder, regardless of
whether the game was chosen by the picker or an explicit CFG. For resources
beside a per-game NEX, set its CFG to `.`. A stale CFG falls back to discovery.
Old `saves/<fingerprint>` folders remain readable when a flat slot is absent,
including the older launch-folder location. Keep both `.a` and `.b` files
when backing up or moving saves. Discovery supports up to 32
games and resource paths up to 100 characters.
Use a current Next core and NextZXOS. The interpreter selects 28 MHz and targets
1 MB RAM. Preserve your existing saves before replacing a testing build.

## Controls

- Arrow keys: move or navigate; two perpendicular arrows move diagonally.
  Press the current direction again to stop.
- Break: open/close the menu or cancel.
- INV VIDEO: inventory. Graph has no inventory action.
- Edit + 1–9, 0: F1–F10, labelled E1–E0 in the game.
- E5: Save; E7: Restore; E9: Restart with confirmation.
- Edit + Q/J: Quit/Joystick.
- Enter: submit/select/dismiss; Delete: erase the previous character.

## Emulator and source builds

Install z88dk and CSpect separately. From the source directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Runtime
powershell -NoProfile -ExecutionPolicy Bypass -File .\run.ps1 -Runtime -Game sq1
```

Use `kq1` or `pq1` for the other games. The default tool paths are `C:\z88dk`
and `C:\cspect\CSpect.exe`; scripts accept `-Z88dkRoot` and `-CSpectPath`
respectively. The launcher prepares emulator save folders and keyboard support.
In CSpect use Escape for Break and tap Tab then the number for Edit shortcuts.
`-FreeRun` runs uncapped, disables emulator sound and changes wall-clock timing;
omit it when checking hardware-like performance.

Use `-Runtime -SelectGame` instead of `-Game` to enable automatic discovery.
This writes `AUTO` to the project CFG. Both discovery and explicit `-Game sq1`
use `games/sq1/saves`. The host launcher copies legacy pairs into empty flat
slots, leaving originals untouched and preserving any existing flat slots.

## What to test

Explore optional SQ1/KQ1 paths and complete the PQ1 playthrough. Check save/restore,
menus, music, cutscenes and controls after restarting. Report the game version,
interpreter build, hardware/core or emulator version, room, actions and outcome.
Include the full STOP line if one appears. A screenshot and the relevant save
can help; do not upload original game resources. Remove personal information
from descriptions and attachments before posting.

Performance varies by room. Recent corridor measurements improved walking
throughput by about 20%; that is not a game-wide speed claim.
See [known issues](known-issues.md) before reporting an existing problem.
In particular, unused slots still show `[Unavailable]` on hardware, although
the user confirms that saving and restoring through them works. The attempted
missing-directory fix passes emulator tests but has not resolved that label.

## PQ1 driving regression

`python tools/prepare_driving_tests.py` copies local PQ1 resources and the
hardware playthrough snapshots into `build/driving-sd`, validating snapshot
CRCs and recording source/copy SHA-256 hashes. It requires the local Ccarrols,
Going to drive, and older parked-at-Carol's saves; these proprietary/personal
fixtures are not distributed. It never changes the originals.

Build with `powershell -NoProfile -ExecutionPolicy Bypass -File
tools/build_banked.ps1 -TestDriving`. Run `build/driving-tests.nex` with CSpect
at 28 MHz, mounting **build/driving-sd**, not the project root. The log is
`build/driving-sd/build/driving-tests.txt`. Freerun is useful here: timings are
emulated frame counts, excluding gameplay pacing. The sixteen-map audit takes
several minutes and emulator file buffering can delay log visibility.

Coverage includes restoring/driving/parking from actual saves, map-label
clearing, per-stage rendering profiles, a car turning at the screen edge,
authored light phases and traffic routes, and the original dispatch responses.
Red-light waits (`observe.blocks`) and authored `stop.update` waits are excluded
from the stuck-car detector. Run the parser/render, motion, sound, save, KQ1 and
PQ1 suites too when changing shared drawing or movement code.

The legacy `-TestRuntime` core/UI suite and `-TestParser` parser/render suite
are separate images. Combining their resident state exceeded the reserved
interrupt boundary as the pixel-oracle coverage grew. Run both when the full
runtime regression set is required. Re-run `tools/prepare_save_tests.py` before
each save suite: the interrupted-generation and label tests require empty
diagnostic slots, and that preparation only clears the isolated test mount.

For renderer profiling, `-ProfileDriving` builds `build/driving-profile.nex`.
It uses the same isolated driving mount and log, and covers both cafe room 12
and the motorway/light junction one map south (room 16). It omits the longer
dispatch and sixteen-map motion audit. Report full-workload totals and worst
updates as well as render time; a faster microbenchmark alone does not prove
better gameplay. Emulator logs may remain buffered until the suite closes them.
