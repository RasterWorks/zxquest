# KQ1 playthrough findings — 14 November 2026

Status: condor catch/flight (item 7) fixed and confirmed by the user on both
CSpect and ZX Spectrum Next hardware. User completed the KQ1 playthrough on
hardware on 14 November 2026. Follow-up changes for items 1, 2, 4, 5 and 8 are
implemented and awaiting hardware confirmation. Items 3 and 6 are investigated
below; original positions and terrain have not been changed.

1. Menu bar overruns screen. Consider removing leading space before Info and
   reducing spacing between Action and Special. Verify SQ1 and PQ1 layouts too.
   IMPLEMENTED: trim authored trailing heading spaces and remove the leading
   inset only if the complete bar needs it. KQ1 now fits all six menus in 40
   columns; SQ1/PQ1 retain their inset. Dropdown shortcuts remain right aligned.
2. Quit label/binding should be Edit+Q; Pause should be Break. Resolve how Pause
   and existing Break menu/cancel behavior should coexist before changing input.
   IMPLEMENTED: recognize KQ1's spaced "Alt Z" spelling as E-Q and label Esc as
   BREAK. Existing Edit+Q dispatches Quit; Break opens the menu and suspends
   gameplay. The Pause menu command retains its separate pause dialog.
3. Descending the tree places Graham centrally: compare original DOS behavior.
   VERIFIED SCRIPT BEHAVIOR: original LOGIC 14, offset 84, explicitly positions
   Graham at (84,132) after a normal return from room 63. The runtime test
   reproduces that exact position. This is script evidence, not a DOS video
   comparison; no arbitrary repositioning correction applied.
4. Consider semicolon, comma and full stop for Swim, Duck and Jump respectively.
   IMPLEMENTED: ; swims, , ducks, . jumps, including bindings restored from
   older saves. Menu/help labels match; normal 0/-/= no longer invoke actions.
5. Growing the beanstalk leaves a Graham afterimage: investigate redraw bounds.
   FIXED: overlay.pic restores the clean background before decoding the overlay
   and capturing the new scene. A live actor must never become part of that
   background. Regression compares the result with an actor-free overlay.
6. Beanstalk safe climbing route appears misaligned: expose a priority/control
   map for comparison against the picture and original game.
   INVESTIGATED: VIEW 74 is eight AGI pixels wide; collision checks the entire
   baseline, not the visible sprite centre. Original P2 boundaries are retained.
   Exported pictures 70–72 with tools/render_navigation_map.py show the priority
   map and reachable trigger-free baseline positions. Lower-room transition
   rows are excluded from reachability (otherwise the map leaks around the
   bottom of the boundary). Upper-room P2 can dismount rather than kill Graham.
   Local outputs: build/kq1-picture-{70,71,72}.navigation.png. These include
   game artwork and must not be included in public source/runtime packages.
7. Condor catches then loses Graham; no flight scene, subsequent walking too fast.
   RESOLVED: user verified the fix on emulator and hardware and finished the game.
   Local save (current flat layout): build/sd-card/ZXQUEST/KQ1/saves/3.a,
   description "Beat dragon, have mirror". Preserve player saves.
8. Ending credits text alignment: "Thank you for playing" currently appears on
   one line and "King's Quest" on the next; these should share a line. User
   believes "from" should also be on that first line and the remaining names
   should form two columns. Check the original DOS layout to confirm the
   placement of "from" and the name columns, then tidy wrapping and alignment.
   IMPLEMENTED: recognize the original credits' space-separated fields and
   retain every name from the game file. Title stays on one 34-character line;
   "from" is centred beneath (including it in the heading needs 39 characters,
   wider than the 38-character bordered interior). Paired names use fixed
   columns; standalone names are centred. Original game resources are untouched.

Investigation: LOGIC 22 catches Graham while end.of.loop jump flag 144 is active,
then starts move.obj.v toward x=135. Original AGI motion and cycle parameters
share storage; starting motion replaces the previous loop completion flag.
ZXQuest's independent fields allowed flag 144 to fire, and LOGIC 0 then restored
walking and cancelled flight without restoring its temporary step size.
Reference semantics: Sarien src/op_cmd.c and src/view.c; independently confirmed
by ScummVM engines/agi/motion.cpp (motionActivated; bug #7046). Reference only;
ScummVM code is not copied into this GPL v2 project.

Source fix prepared: a spare actor flag suppresses completion of a one-shot
animation superseded by move/follow/wander. A newly requested one-shot clears
that marker. Actor size and snapshot layout are unchanged. Test entry point:
`powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_banked.ps1 -TestKq1`.
The harness stages the original LOGIC 22 catch branch and checks flight room 80,
landing room 48, player control and walking step, plus normal loop completion.

Validation: compiler now runs. Production and diagnostic builds pass bank and
resident bounds checks (production retains 512 resident bytes free). The KQ1
harness passes all 10 checks, including rooms 22 -> 80 -> 48 and normal walking
after landing. Movement, follow/wander and SQ1 room regressions pass 142 checks.
CSpect crashes during sandboxed startup on this host; tests run successfully
outside the sandbox. Compiler temporary files use build/compiler-temp for this
session. All 46 save checks pass with freshly generated isolated fixtures;
SQ1/KQ1/PQ1 compatibility completes with zero failures. Reused save fixtures
initially caused five failures; regeneration resolved all five.
Condor-only build previously copied to the SQ1/KQ1/PQ1 SD-card folders:
SHA-256 6996C5760D4C07BFC79541A7146EE9D266CC3BAF386E5B507CA3C42507B76B51.
Previous SD-card executables are backed up in build/pre-condor-fix.
Game resources, configuration and player saves are unchanged.
The condor-only patch made no menu/key changes; the follow-up above does.

Follow-up validation (14 November 2026): KQ1 27/27; SQ1 UI 30/30;
motion 60/60; follow/wander 54/54; room traversal 28/28; clean save tests 46/46.
Total: 245 checks, no failures. SQ1/KQ1/PQ1 compatibility also reports zero
failures. Menu captures for all three games and KQ1 credits were visually
inspected. The production build fits all banks and retains 512 resident bytes.

Latest build/runtime.nex and all three SD-card ZXQUEST.NEX copies:
SHA-256 EACD3F2C11892189D71E0ADA525976A82D6E152191DD6DFD5FAB53C5259EF07B.
Previous executables are in build/pre-kq1-layout-fixes. Player saves, config
and game files were preserved. Hardware confirmation of the follow-up fixes
is still required; test planting from before the overlay, since an old snapshot
can retain an already-baked afterimage until the room is redrawn.
