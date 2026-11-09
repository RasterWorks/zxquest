# ZXQuest

A Sierra AGI v2 interpreter for the ZX Spectrum Next, built in C and Z80N
assembly with z88dk. It runs the original game scripts and resources from
separate files. Target: a base 1 MB Next running NextZXOS at 28 MHz; no Pi required.

## Public testing preview

**Space Quest I has been played through to the ending on real Next hardware.**
That playthrough scored 184/202, so optional paths still need testing. King's
Quest I and Police Quest I have passed automated startup, movement, LOOK and
save/restore checks, but have not yet had full manual playthroughs. Use the
original DOS/EGA AGI versions, not later SCI or VGA remakes.

The interpreter supports graphics, animation, collision, room transitions,
typed commands, menus, inventory, AY music/effects and save/restore. This is a
testing preview, not a claim of complete compatibility with every AGI game.

**Original Sierra game files are not included.** Supply your own copies.
[Hardware installation and testing](docs/testing.md) explains setup, controls
and useful bug reports. See [CONTRIBUTING.md](CONTRIBUTING.md) to help test or develop.

## Licence and attribution

ZXQuest is distributed under the GNU General Public License version 2; see
[COPYING](COPYING). Adapted Sarien code retains its original copyright and GPL
notices, including those of Stuart George and Claudio Matsuoka. The upstream
revision and adaptation details are recorded in
[interpreter foundation](docs/interpreter-foundation.md).

The licence covers the interpreter project, not Sierra's original games.
Game resources, personal saves, CSpect and z88dk are not part of the public
package. Binary distributions must be accompanied by the corresponding
source under the GPL; public releases should provide both archives together.

## Build and run (PowerShell)

```powershell
.\build.ps1 -Runtime
.\run.ps1 -Runtime
# Same executable, different original AGI game files:
.\run.ps1 -Runtime -Game kq1
.\run.ps1 -Runtime -Game pq1
.\run.ps1 -Runtime -Game sq1
# Uncapped host speed (omit -FreeRun for realistic 28 MHz timing):
.\run.ps1 -Runtime -Game kq1 -FreeRun
# Separate sprite/picture viewer:
.\build.ps1
.\run.ps1
```

On the Next, **Break** opens/closes menus or cancels, and **INV VIDEO** opens
Inventory. Hold **Edit + 1-9, 0** for Sierra F1-F10; labels **E1-E0** mean these
Edit shortcuts. **E5** saves, **E7** restores, **E9** restarts (confirmation),
and **Edit + Q/J** selects Quit/Joystick. Edit alone is only a modifier.

**Arrow keys** move Roger or navigate menus and save slots. Two perpendicular
arrows choose a diagonal. Press the same direction again to stop. Staggered
releases retain the chosen diagonal; release all arrows before a new press.
Opposing arrows cancel that axis. **Enter** selects/submits/dismisses;
**Delete** removes the previous typed character. **Space** skips the SQ1 title.

In CSpect, the launcher enables BASIC keyboard mode: PC arrows navigate and
**Escape** opens/cancels the game menu. Tap **Tab**, then a number for function
shortcuts (**Tab,5** Save; **Tab,7** Restore), or Q/J for Quit/Joystick.
The dedicated Next keys are the primary hardware controls. Extended Mode,
Caps Shift+Symbol Shift, and classic cursor chords remain compatibility aliases.
See [keyboard notes](docs/keyboard-plan.md) for details and validation status.

Save/Restore offers nine vertical slots. Up/Down chooses a slot. Saving accepts
a description of up to 31 characters before confirmation; the centred
Saving notice remains until the write has been verified. Saves are kept
outside the original game data and are not Sierra DOS saves.

Scripts default to `C:\z88dk` and `C:\cspect\CSpect.exe`. Override with
`-Z88dkRoot` or `-CSpectPath`. The build sets PATH and ZCCCFG only for its
current process. No system environment changes are needed.

`-FreeRun` passes CSpect's `-freerun` option: emulation runs as fast as the host
allows with the emulated CPU still configured at 28 MHz. Game timers and
animations advance faster too. CSpect requires audio disabled in this mode,
so the launcher also passes `-sound`.

Keep the original game files in `games/sq1/`:

```text
LOGDIR PICDIR VIEWDIR SNDDIR VOL.0 VOL.1 VOL.2 WORDS.TOK OBJECT
```

Game files and all generated outputs are ignored by Git. They are never
embedded in the executable. CSpect mounts the project directory as its SD
root, so no SD image or game conversion is needed for development. A real
Next needs the same paths on its SD card; SQ1 has been completed on hardware.
This interpreter targets AGI v2 games; SCI remakes and AGI v3 compressed resources
are not supported. KQ1 and PQ1 startup and save tests use their original files;
see [compatibility](docs/compatibility.md) for the scope and file findings.

Viewer controls:

| Keys | Action |
| --- | --- |
| **WASD** (hold) | Move the sprite and cycle walking cels |
| **C**, **L** | Next cel / next loop |
| **V**, **T** | Next / previous VIEW resource |
| **H** | Hide or show the sprite |
| **N**, **B** | Next / previous room picture |
| **P** | Visual / priority map |

Missing resource numbers are skipped. Movement stays within the screen but
can pass through walls: collision/game logic comes later. CSpect's Escape
closes the emulator.

## Validation

The combined runtime suite exercises scripts, actors, menus, modal input,
all 738 original dictionary entries, and SQ1 startup through first-room commands:

```powershell
python tools/prepare_runtime_tests.py
.\build.ps1 -RuntimeTest
.\run.ps1 -RuntimeTest
```

Completion is `SUMMARY ... failures=0` in `build/runtime-tests.txt`. The test
exports title/intro/first-room pixels under `build/`. The parser subset also
writes `build/parser-tests.txt`; run it separately with `-ParserTest` on both
scripts. Inventory selection and original item-inspection scripts have their own suite:

```powershell
python tools/prepare_inventory_tests.py
.\build.ps1 -InventoryTest
.\run.ps1 -InventoryTest
```

Require passing summaries in `build/inventory-tests.txt` and
`build/inventory-game-tests.txt`. Collision and original-room traversal have a separate suite:

```powershell
.\build.ps1 -MotionTest
.\run.ps1 -MotionTest
```

Require fresh passing summaries in `build/motion-tests.txt` and
`build/room-tests.txt`. See [collision notes](docs/collision.md) and
[validation](docs/validation.md).

The core VM suite and earlier startup diagnostic remain separate builds:

```powershell
python tools/prepare_logic_tests.py
.\build.ps1 -LogicTest
.\run.ps1 -LogicTest
# The same core tests with VM code in an 8 KB overlay:
.\build.ps1 -BankedLogicTest
.\run.ps1 -BankedLogicTest
# Original SQ1 startup, stopping at its first unsupported command:
.\build.ps1 -Logic
.\run.ps1 -Logic
```

The test writes `build/logic-tests.txt`. It exercises authored bytecode and
checks all 2,137 messages in the 101 supplied LOGICs against independent host
checksums. The earlier minimal diagnostic calls LOGIC 104 and stops explicitly at
`trace.info`, offset 8. The combined `-Runtime` target implements startup beyond
that earlier checkpoint.

```powershell
.\build.ps1 -Test
.\run.ps1 -Test
python tools/inspect_game.py games/sq1 --json build/game-inventory.json
```

The test NEX runs synthetic decoder cases and decodes every existing picture
in the supplied game. It compares every packed visual/priority pixel against
the original decoder and checks the actual Layer 2 pixels for three rooms.
It writes `build/verify.txt` and `build/pic001.raw`
through the emulated filesystem. Wait for `SUMMARY` in the log; an empty or
partial log is not a passing result. Tests run with CSpect's frame limiter
disabled. The normal viewer retains normal emulation timing.

`python tools/render_dump.py` turns the target-generated pixel dump into a PNG
(Pillow required). This is an inspection artifact, never an input to the game.

Performance measurements are reproducible with `./build.ps1 -Benchmark` and
`./run.ps1 -Benchmark`. Results are written to `build/benchmark.txt`, in
emulated 50 Hz frames (20 ms each); host filesystem latency is excluded.
See [performance notes](docs/performance.md) for the initial measurements.

The separate resource suite runs much faster than the picture regressions:

```powershell
python tools/prepare_resource_tests.py
.\build.ps1 -ResourceTest
.\run.ps1 -ResourceTest
```

It generates fixtures under `build/`, verifies host-computed CRCs for all 462
readable game resources, and tests bank boundaries, pinning, eviction, stale
handles and corrupt files. Completion is recorded in `build/resource-tests.txt`.
Regenerate the manifest whenever the supplied game files change. The known
missing `VOL.15` reference in `SNDDIR` entry 0 is an expected error.

The VIEW/compositor suite checks all original cels and sprite drawing:

```powershell
python tools/prepare_view_tests.py
.\build.ps1 -ViewTest
.\run.ps1 -ViewTest
python tools/render_dump.py build/sprite-room.raw build/sprite-room.png
```

The host tool independently decodes 1,652 cels (216 mirrored) in 238 VIEWs
and writes checksum expectations plus a contact sheet under `build/`. The
Next test writes `build/view-tests.txt` and exports the composited room.
See [VIEW rendering notes](docs/view-rendering.md) for API contracts and limits.

## Design and next steps

See [the architecture notes](docs/architecture.md) for the memory layout,
current limits, source references and the path toward a reusable interpreter.

The adapted Sarien VM carries its original copyright/GPL notices. The upstream
license is in [COPYING](COPYING); source provenance is recorded in the VM notes.
