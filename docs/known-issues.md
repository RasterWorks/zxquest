# Known issues — 16 November 2026

## Next correction round: unused save-slot labels

Unused slots still show `[Unavailable]` on Next hardware. The user confirms
that saving to and restoring from these slots works. This remains an open
label/enumeration problem, not a confirmed failure to store or load saves.

The latest build handles missing-parent `ESX_EPATH` as well as `ESX_ENOENT`.
All nine labels in both dialogs pass the emulator error-injection checks, but
the hardware report shows that this correction is insufficient. Next steps:

- Capture the actual lookup path and filesystem return code during hardware
  label enumeration, including flat and legacy fallback paths.
- Compare that lookup with the successful save/restore operation.
- Add a regression for the observed cause and retest both dialogs on hardware.

Keep genuine disk/access errors visible; do not hide every error as an empty
slot. No new correction is included in this documentation update.

## PQ1 driving can still pause

Spread-out traffic and traffic-light changes can be slow. The latest matched
room-16 benchmark uses about 20% less CPU time, with its worst update reduced
from 1.02 to 0.82 seconds at emulated 28 MHz. That still leaves visible pauses.
Further work should profile script execution and actor-dependency searches
using the same road/light workload, preserving authored motion and timing.
See [performance measurements](performance.md) for tradeoffs and test scope.

The previously reported stale map label after a driving restore has not been
reproduced by the tested restore/parking paths. A matching source/destination
save and map sector are needed before attributing it to a particular path.

## Compatibility and remaining playthrough work

- SQ1 and KQ1 reached their endings on Next hardware. SQ1 scored 184/202;
  optional routes and other game editions still need testing.
- KQ1's later menu, credits and beanstalk-afterimage corrections pass automated
  checks but still need explicit hardware confirmation. An old save can retain
  an already-captured afterimage until the room is redrawn.
- PQ1's hardware playthrough is in progress. Existing parking-lot snapshots can
  retain the old collision map; leave and re-enter to regenerate it.
- Only the tested DOS/EGA AGI v2 editions are covered. AGI v3 compression and
  SCI games are not supported. Other AGI v2 games are not yet validated.

Report the build, game edition, core/NextZXOS version, location and steps. See
[testing instructions](testing.md); do not upload original game resources.
