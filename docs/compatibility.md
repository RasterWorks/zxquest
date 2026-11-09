# AGI game compatibility — 25 October 2026

The same `build/runtime.nex` is used for all three games; no game-specific
source build or resource conversion is required. The optional `ZXQUEST.CFG`
contains a relative game directory, defaulting to `games/sq1` when absent.
`run.ps1 -Runtime -Game sq1|kq1|pq1` writes that setting and prepares the game's
save folder before starting CSpect at capped 28 MHz.

| Supplied game | Original interpreter string | Opening room reached | Tested |
| --- | --- | --- | --- |
| Space Quest I | 2.917 | 2, opening corridor | Startup/name, movement, look, save/restore, resumed gameplay |
| King's Quest I | 2.425 | 1, outside castle | Startup, movement, look, save/restore, resumed gameplay |
| Police Quest I | 2.911 | 6, station lobby | Startup, movement, look, save/restore, resumed gameplay |

All three also invoke the native slot dialogs from their original F5/F7 script
bindings. The harness injects PC key events directly; this does not claim that
host F5/F7 keys are mapped in CSpect. Interactive players can use the game's
menus; the [Next key plan](keyboard-plan.md) covers the forthcoming keyboard layer.
These are opening-room integration checks, not complete-game playthroughs.
Later scenes can still encounter an unsupported AGI instruction or behavior.

## File inspection

`tools/inspect_game.py` validates directory entries and volume headers without
modifying them. Reports are under ignored `build/*-inspection.json`.

| Game | Valid LOGIC / PIC / VIEW / SOUND resources | Findings |
| --- | --- | --- |
| SQ1 | 101 / 73 / 238 / 50 | Existing unused SOUND 0 entry refers to absent VOL.15 |
| KQ1 | 90 / 82 / 118 / 22 | SOUND 34–37 point beyond supplied VOL.2 |
| PQ1 | 118 / 71 / 220 / 36 | No directory/header issues found |

The KQ1 sound offsets are 134703, 134799, 134893 and 135787; the supplied VOL.2
is 90,891 bytes. These entries were not needed by the tested opening path.
Do not silently repair the directory or substitute game data. Later use of one
of those sounds may require a complete matching copy of that game edition.
All **92 supplied files**, including non-resource extras, retained their original
SHA-256 hashes during this milestone (`build/save-source-hashes.json`).

Scope remains **AGI v2**: SCI versions/remakes are a different interpreter,
and AGI v3 compression is not implemented. Earlier AGI releases can also differ
in opcode arity/semantics. The two additional tested editions use the current
decoder successfully on these paths; this is not blanket support for every AGI
release or every Sierra game.

## Evidence and reproduction

See [save/restore](save-restore.md) for the isolated `-SaveTest` SD folder and
commands. `build/save-sd/build/compatibility-tests.txt` reports zero failures for
all three games. It includes the exact room, LOGIC, instruction offset and
opcode at each checkpoint; the three `*-startup.l2` files are framebuffer dumps.
The test suite also verifies 30 snapshot/error/lifecycle checks separately.
