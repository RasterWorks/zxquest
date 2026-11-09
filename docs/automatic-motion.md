# Following and wandering

The interpreter implements `follow.ego` (0x53) and `wander` (0x54) for all
256 actor IDs. Decisions run only for animated, drawn, updating actors when
their step timer expires. They feed the existing movement/collision path;
normal movement and `move.obj` keep their separate handling.

## Follow

`follow.ego` clears the script's completion flag, enables updates and stores
a distance at least as large as the actor's current step. It compares the
horizontal centres of the actor and ego at their baseline heights. Both
coordinate differences must be strictly smaller than the distance to arrive.
Arrival stops motion and raises the requested flag without changing step size.
The flag is observed on the eligible decision tick after movement reaches
the threshold, not retroactively during the preceding movement.

A blocked follower chooses a nonzero random direction and a bounded detour
distance based on its separation from ego. After that detour it resumes
pursuit. This is AGI-style obstacle recovery, not route-finding: a follower
can remain trapped by geometry. It still obeys terrain, water/land, actors,
horizon and block rectangles, subject to the game's usual ignore commands.

## Wander and control

Wander retains its direction until its countdown expires or a scheduled
movement fails. It then chooses a direction in 0..8. A short countdown is
refilled in 6..50 for both ego and other actors. The original byte-counter
zero-to-255 decrement behaviour is retained. An edge emits the usual room
event but does not falsely complete an unrelated `move.obj` flag.

ZXQuest deliberately treats a boundary-clipped wandering step as blocked even
when the other axis moves. This is a small departure from AGI's whole-position
stall test: otherwise diagonal wander can slide along the horizon or screen
edge until its byte countdown expires. In SQ1's rock-bridge room this produced
203 consecutive ticks on one Y coordinate. A clipped step now requests a new
direction on the next scheduled step. Interior diagonals, terrain collision,
follow, scripted moves, countdown ranges and the save-state layout are unchanged.

Wandering ego switches to program control and keeps direction variable 6
current. `player.control` now cancels ego's automatic motion; normal keyboard
movement can resume. `normal.motion`, `stop.motion` and `start.motion` cancel
automatic modes without reporting a successful follow. Room changes reset
motion and stall state. Erased/stopped actors do not consume random numbers
or advance automatic-motion timers.

## Memory and adaptation

The 32-byte actor record is unchanged. Motion values are normal=0, move=1,
follow=2, wander=3. Follow reuses target_x for distance and target_y for its
detour countdown; wander reuses target_x as its countdown. Bit 16 in the
secondary flags records a failed scheduled move. Old sprite state stays
independent of this movement state.

Direction decisions use overlay 95 through a resident call gate, so ordinary
movement does not switch into that overlay. The gate restores MMU4 and the
direction code restores MMU3 through the existing actor-read API. Collision
does not remap the cached MMU2 picture window.

The implementation adapts the pinned Sarien `motion.c` and command semantics;
see [source provenance](interpreter-foundation.md). Current ScummVM's
[motion implementation](https://github.com/scummvm/scummvm/blob/master/engines/agi/motion.cpp)
was consulted as a behaviour reference for the non-ego wander countdown.
Random selection uses ZXQuest's existing 16-bit generator and bounded ranges,
not rejection loops; the random sequence is not intended to match DOS AGI.
Zero distance with zero step uses a distance of one, and detour countdowns
saturate at zero. Version-specific motion/cycler parameter-aliasing quirks
remain outside this milestone.

## Checks

`build.ps1 -MotionTest` includes `tests/follow_tests.c` in test overlay 57,
in addition to collision and room-transition suites. It writes
`build/follow-tests.txt`. Require fresh zero-failure summaries in all three
logs. Authored checks cover eight directions, threshold/centre semantics,
obstacle detours, delayed/stopped/hidden motion, zero-step bounds, wandering
and borders, ego control, VM dispatch and unchanged record size/bank mappings.
The original LOGIC 110 droid fixture stages its flags and coordinates and
executes the supplied scripts/views; this does not represent a playthrough
to that encounter. Full runtime/parser/rendering regressions are separate.

## SQ1 return-from-caves arrival timer

In the supplied game's LOGIC 15, returning to the surface from below arms
flag 96 and variables 94/95 when destruction flag 161 is clear. It does not
check whether a droid is already active. LOGIC 0 consumes that timer and sets
arrival flag 101. The successful rock crush in LOGIC 19 sets flags 161/165
and clears 101/97, but does not clear 96. Thus an already queued arrival can
produce another droid after a successful crush. The player's pre-crush slot 9
had both flags 96 and 97 set, consistent with this route through the scripts.
This is game-authored state, not evidence that the crush failed or a reason
to change generic actor lifecycle semantics.

The Kerona diagnostic stages the original crush branch with and without an
arrival timer, and runs the original LOGIC 0 timer block plus room 19 afterward.
It also logs entries to the script's exact crush zone (X=36..58, Y=137..151),
the first entry, and longest absence during a 1200-cycle wandering sample.

## Scripted walking direction

`move.obj` and `move.obj.v` now set the actor direction immediately and
refresh it on eligible movement ticks. For Roger this also updates AGI
variable 6, allowing the original game scripts to start and stop walking
animation. The interpreter does not force cycling flags. Other actors do
not overwrite Roger's direction. Arrival still clears direction, restores
the original step size and returns control to the player.

Validation: `powershell -ExecutionPolicy Bypass -File tools/build_banked.ps1 -TestCloset`
builds `build/closet-tests.nex`. Run with the project as the SD root. The
suite writes `build/motion-tests.txt` and `build/closet-tests.txt`; it does
not modify saves. The original SQ1 closet exit produced 23 moving samples
and 22 cel changes (previously zero), and all 60 collision/movement checks
passed, including diagonal partial steps and non-ego direction isolation.
