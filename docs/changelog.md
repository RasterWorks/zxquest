# Changes and tests

## 16 November 2026 — update since the initial public preview

This update publishes the accumulated interpreter, test and tooling changes.
Original Sierra resources and personal saves are not included. The runtime
continues to target a base 1 MB Next at 28 MHz, with the same save format.

### Gameplay and display corrections

- KQ1: corrected condor catch/flight completion and walking speed after landing.
  The user completed the game on Next hardware with this fix.
- KQ1: fit all menu headings, update Quit/Pause labels and Swim/Duck/Jump keys
  (`;`, `,`, `.`), prevent Graham being captured into the growing-beanstalk
  background, and align the ending credits. A navigation-map export tool helps
  inspect the original climbing boundaries without changing game resources.
- PQ1: support the newspaper's text operation, complete solid/textured picture
  brushes for the police-station exterior, and correct parked-car occlusion.
- PQ1: apply cel-size placement correction before traffic movement, preventing
  false route completion when a narrow car turns at the edge of the screen.

### Setup, saves and speed

- Discover game folders beside the NEX or under `games/`. Load one directly or
  offer an arrow/Enter picker for several. Explicit CFG paths remain supported;
  missing/stale paths fall back to discovery.
- Store saves directly in each game's `saves/` folder. Retain legacy-folder
  lookup, preserve paired generations, and reject foreign-game overwrites.
- Enable the games' Speed settings through pacing at 28 MHz. Fastest adds no
  deliberate delay; movement distances and animation rules are unchanged.
- Reduce VM/resource overhead and avoid redundant sprite/background work.
  Separate actor-dependency search bounds from actual paint bounds and reuse
  control-run geometry. Keep renderer scratch separate from IRQ sound data.

### Measured performance

The latest follow-up compares the previously shipped flat-save build with the
new renderer in CSpect at emulated 28 MHz, excluding intentional pacing waits:

| Authored workload | Before | After |
| --- | ---: | ---: |
| PQ1 cafe map, 280 updates | 2,725 frames | 2,544 frames |
| PQ1 motorway map one screen south, 280 updates | 6,737 frames | 5,420 frames |
| Worst motorway update | 51 frames / 1.02 s | 41 frames / 0.82 s |

The motorway workload takes about 20% less CPU time. Some stalls remain, and
a simple same-row microbenchmark is slightly slower. These are focused
emulated measurements, not whole-game FPS or confirmed hardware gains.
See [performance](performance.md) for earlier comparisons and limitations.

### Validation

The final runtime's focused run passed **150 checks**: render 26, parser 38,
save 46, save layout 24 and driving 16. SQ1/KQ1/PQ1 opening, movement, menus
and save/restore integration also reported zero failures.

Earlier stages additionally exercised KQ1's condor/credits/overlay scenarios,
PQ1's newspaper and all 71 pictures, 128 brush footprints, parked-car collision,
motion/follow/room traversal, audio, speed settings and discovery. Those were
separate dated runs; they were not all repeated against the final renderer.
The full sixteen-map traffic audit passed on the baseline; the final focused
driving run covers the cafe and motorway/light junction. Exact counts, evidence
and reproduction instructions are in [validation](validation.md) and
[testing](testing.md).

Production fits all banks, retains 259 resident bytes before IM2/stack, and
has SHA-256 `F53DB7C5DA85789E5D7FA88B3CA27F0B0461C0B291DAE973E06707FE1383FC73`.
All four local SD-bundle executables match. All 76 other bundle files, including
44 personal save snapshots, were verified unchanged. Those files remain local.

### Still open

**Unused slots still show `[Unavailable]` on hardware, although saving and
restoring through them works.** The missing-directory handling passes emulator
checks but did not resolve the user's hardware report. This is recorded for the
next correction round; it is not a completed fix. PQ1 driving stalls also need
further work. See [known issues](known-issues.md).
