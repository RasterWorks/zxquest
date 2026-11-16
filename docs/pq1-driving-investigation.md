# PQ1 driving investigation — 16 November 2026

The installed z88dk toolchain now compiles successfully following the user's
Windows configuration change. The interpreter changes below have been built,
validated and copied to all four executables in `build/sd-card/ZXQUEST`.

## Reproduction and changes

The user's **Ccarrols** snapshot is at
`build/sd-card/ZXQUEST/PQ1/saves/5.a`. It is driving room 12 at Fastest,
with accident state V92=255 and current scenario V67=1 (after the accident).
Tests use verified copies in `build/driving-sd`; originals are never written.

* Separated cars previously expanded one redraw rectangle across unrelated
  building control columns. Each dirty vertical band now has independent
  horizontal bounds, with whole-actor and control-ray closure retained.
* Tall control columns were rescanned for every VIEW pixel. A per-blit cache
  reuses their effective priority, halving cafe-map initialization time in the
  measured workload. It lives in unused picture-page tail space, separate
  from sound streams. An uncached reference reader checks all priorities.
* Hidden permanent stamps defer actor composition until `show.pic`; the clean
  background remains current. Restore invalidates the derived control map.
* A narrow vertical car reused at the right screen edge could become wider
  when turning left. The old engine treated the resulting clipping as route
  completion, advancing to a diagonal leg from the wrong place. Cel changes
  now apply the AGI placement correction before movement. A focused original
  traffic-VIEW test checks the completion flag and subsequent movement.

The last change follows [Sarien set_cel](https://github.com/cmatsuoka/sarien/blob/master/src/view.c)
and its UPDATE_POS handling in [checks.c](https://github.com/cmatsuoka/sarien/blob/master/src/checks.c).
No room coordinates, game identifiers or resource numbers are hardcoded in the
production fixes. Snapshot format and movement step sizes are unchanged.

## Findings that do not require game-script changes

**Traffic lights:** original LOGIC 117 counts 255 interpreter updates through a
long phase; short phases start the counter at 245 or 250. Rendering improvements
reduce the wall-clock wait when CPU work exceeds the chosen speed interval.
Normal pacing can still make the authored long phase last about 51 seconds.
Fast/Fastest can shorten it. No light counter or script timing was altered.

**Dispatch:** the original LOGIC 96 driving branch at offset 555 tests whether
V92 equals **199**, selecting the location reminder and en-route reply only in
that case. Other values take the 10-98 reply. Executing the original script
with V92=0, 199 and 200 confirms this, including 10-98 before arrival at state 0.
An earlier static interpretation of this branch as a less-than comparison was
incorrect. The regression now checks the authored branches, not an assumed
radio procedure. No interpreter or original-resource workaround is applied.
The supplied Ccarrols save is after the accident and cannot reproduce the
user's exact earlier drive.

**Map label:** restoring Ccarrols shows Map A3; restoring the station-exterior
snapshot then removes it. Restoring the older parked-at-Carol's snapshot and
typing `park` also removes it on entry to room 35. The reported stale label has
not been reproduced by these paths, so no speculative text-clearing change was
made. Record the source/destination save and sector if it happens again.

**Traffic waiting:** room scripts use `observe.blocks` to stop cars at blue
control lines for red lights. LOGIC 115 also deliberately stops nearby cars
with `stop.update`. These waits are excluded from stuck-car detection. The
route audit covers 420 updates on each of sixteen driving maps, with ego erased
to avoid player interference; it does not replace a full driving playthrough.

**Carol's:** the user confirmed `park` works. No change is needed.

## Evidence and validation

`tools/prepare_driving_tests.py` verifies snapshot CRCs and resource/save hashes,
then prepares the isolated test mount. Build `-TestDriving`, mount
`build/driving-sd`, and inspect its `build/driving-tests.txt` after completion.
The final route, sound and cross-game tests passed before refreshing the SD
package. There are 366 passing checks plus the three-game integration run.
All four NEX copies match SHA-256
`08F8DB423936F07716551660F4482F3C9022AE04FFAE65C42CCD84164A8BF770`.
The other 76 package files were verified unchanged. See `docs/performance.md`
and `docs/validation.md` for details. Hardware confirmation is still required.

Initial comparison logs and the previous production executable are retained in
`build/pre-pq1-driving/`. The initial diagnostic already included deferred
hidden stamps and partial band work, so it is not an unmodified release
baseline. Its traffic failures and corrected test assumptions are retained in
`route-audit-before.txt` and `edge-fix-cafe.txt` for investigation provenance.

Build shell: set TEMP and TMP to the workspace's `build/compiler-temp` (no
spaces), then use `powershell -NoProfile -ExecutionPolicy Bypass -File
tools/build_banked.ps1 -TestDriving`. Do not build variants concurrently.

## Follow-up: spread-out traffic south of the cafe

The report identifies room 16, with motorway cars and a traffic-light change.
Conservative control-column dependency bounds were also being used as paint
bounds, restoring/uploading unchanged background between affected actors.
The revised compositor separates those bounds and caches control-run geometry
during dependency search. The matched 280-update room-16 workload falls from
6,737 to 5,420 emulated frames; its worst update falls from 51 to 41 frames.
This is an improvement, not a claim that all driving stalls are gone.
See the follow-up table in `docs/performance.md` for tradeoffs and evidence.
