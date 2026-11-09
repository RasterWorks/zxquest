# Hardware installation and testing

This preview contains the interpreter only. Supply your own original DOS/EGA
AGI game resources. SQ1 has been completed on a real Next; KQ1 and PQ1 need
full manual playthroughs. Other AGI games are not yet validated.

## Install

1. Extract the runtime archive into a folder on your SD card.
2. Create `games/sq1` inside that folder and copy your SQ1 resources into it:
   `LOGDIR`, `PICDIR`, `VIEWDIR`, `SNDDIR`, `OBJECT`, `WORDS.TOK` and every `VOL.*`.
3. The supplied `ZXQUEST.CFG` contains `games/sq1`. For KQ1 or PQ1 change this
   single line to your chosen relative game folder, for example `games/kq1`.
4. Open that folder in the NextZXOS browser and launch `ZXQUEST.NEX`.

The same executable runs all three games. You may keep a separate installation
folder and configuration for each. Saves are written under `saves` relative to
the launch folder; keep both `.a` and `.b` files when backing them up.
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

## What to test

Explore optional SQ1 paths and complete KQ1/PQ1 playthroughs. Check save/restore,
menus, music, cutscenes and controls after restarting. Report the game version,
interpreter build, hardware/core or emulator version, room, actions and outcome.
Include the full STOP line if one appears. A screenshot and the relevant save
can help; do not upload original game resources. Remove personal information
from descriptions and attachments before posting.

Performance varies by room. Recent corridor measurements improved walking
throughput by about 20%; that is not a game-wide speed claim.
