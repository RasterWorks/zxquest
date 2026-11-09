# Collision and room transitions

The reusable runtime reads collision rules from the game's clean picture
priority map and executes original room scripts. No SQ1 exits, door rules or
walkable areas are embedded in the interpreter.

## Movement rules

The feet span `x .. x + width - 1` on baseline `y`. Priority 0 always blocks;
priority 1 blocks unless `ignore.blocks` is active; priority 2 triggers ego
flag 3. Flag 0 is true only when the whole baseline is priority 3 (water).
`object.on.water`, `object.on.land` and `object.on.anything` control terrain
restrictions (ego restrictions expire after the tick; other actors retain them).
Fixed priority 15 bypasses the picture rules, retaining screen
and object checks. Sprite-composited priority pixels are never used here.

Drawn, animated actors with overlapping horizontal extents cannot share or
cross baselines. Either actor's `ignore.objs` exempts the pair. Body rectangles
may overlap at different baselines. Tick-start baselines are saved before any
actor moves, including stationary obstacles. All new loop/cel dimensions are
selected before movement, so animated footprints are current for every actor.
`distance` uses baseline-centre Manhattan distance, saturates at 254, and
returns 255 for undrawn actors.

Block rectangles use the strict interior and prevent entering/leaving that
interior. A blocked rectangle crossing stops direction (and ego v6), as in
the pinned Sarien reference. Picture barriers retain the attempted direction.
Movement follows AGI's candidate-baseline checks; it does not introduce a
continuous physics sweep across every pixel of a large step.

`draw`, `reposition`, `reposition.to` and its variable form adjust invalid
placements with a west/south/east/north expanding spiral. Signed temporary
coordinates avoid wraparound. Search is finite; impossible placement returns
`VM_ARGUMENT`. Reposition skips the next scheduled automatic step.

`add.to.pic` and `add.to.pic.v` decode an original VIEW cel into the clean
background, preserving it across sprite restoration. They support automatic
priority and optional control margins, without baking existing actors into
the picture. Margins preserve earlier control pixels and water.

## Room lifecycle

Successful edge contact sets v2 for ego, or v4 (actor ID) and v5 (edge) for
other actors. Edge codes are top 1, right 2, bottom 3, left 4. Top respects
the horizon unless ignored, then the full sprite height. Rejected collision
moves clear their candidate edge event. The next LOGIC scan receives the
event; the subsequent movement tick clears it. A directed move reaching an
edge signals its completion flag and restores its previous step size.

`new.room` and `new.room.v` release executing LOGIC pins, set current/previous
room and ego-view variables, clear scan starts, reset actors and timing,
restore the default horizon and disable block rectangles. The old picture
is marked invalid until the new script draws one. Ego enters at the opposite
edge. Flag 5 stays set through destination initialization and clears after
that scan finishes. The scripts remain responsible for doors and exits.

## Memory and checks

Actor records remain 32 bytes. The former `ignore_blocks` byte now holds
secondary flag bits. Tick-start baselines use `$6500–$65ff` in scratch page
91, above renderer scratch. Collision shares actor code page 54; rendering
has moved to page 92 through its existing resident gate. All data-window
mappings are restored, and collision never changes the picture window's
cached MMU2 mapping.

```powershell
python tools/prepare_runtime_tests.py
.\build.ps1 -MotionTest
.\run.ps1 -MotionTest
```

The suites write `build/motion-tests.txt`, `build/follow-tests.txt` and
`build/room-tests.txt`. Require fresh zero-failure summaries in all three. The original runtime,
parser and rendering regressions remain available through `-RuntimeTest`.

Rules were adapted from the pinned Sarien `src/checks.c`, `src/motion.c`,
`src/cycle.c`, `src/op_cmd.c` and `src/sprite.c`; revision and license are recorded in
[interpreter-foundation.md](interpreter-foundation.md). Follow/wander now use
these collision rules; see [automatic-motion.md](automatic-motion.md).
Inventory UI, save/restore and sound synthesis remain separate work.
