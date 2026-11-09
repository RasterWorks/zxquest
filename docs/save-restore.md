# Save and restore

Implemented 25 October 2026 for the banked interpreter. `save.game` (0x7d)
and `restore.game` (0x7e) open a vertical list of nine slots. Up/Down selects a
slot, including wrapping at the ends; 1–9 remains an optional shortcut. Enter
opens a short editable description when saving, or restores the selected slot.
Type up to 31 characters; Delete/Shift+0 removes a character. The second Enter
confirms the save. An empty description uses the automatic room/score/time label.
Existing descriptions remain editable, and old save files remain readable.

While writing/verifying, the game scene reappears with a small transient
“Saving...” label. The VM stays paused. Completion removes that label and
resumes the game; failures remain visible until Enter or Shift+1. Shift+1 also
cancels selection or editing. Labels use the existing checksummed 32-byte header
field, so this UI change does not change the snapshot format.

The games' own key/menu bindings decide which commands save and restore.
Edit + 5 and + 7 (E5/E7) send F5/F7 (CSpect: Tab, then 5 or 7); see
[keyboard notes](keyboard-plan.md).

## Storage and recovery

Saves live outside game data, in `saves/<game-fingerprint>/`. Each slot has
two files, for example `1.a` and `1.b`. The newest valid generation is used;
the other file retains the previous save. Writing overwrites only the older
or invalid generation, then closes and rereads it to check its checksum. A
partial file is rejected and restoration can fall back to the other copy.
This protects against an interrupted individual write, not physical media or
filesystem corruption affecting both files.

The game fingerprint is CRC32 of the six directory/dictionary/object files
(`LOGDIR`, `PICDIR`, `VIEWDIR`, `SNDDIR`, `OBJECT`, `WORDS.TOK`), including their
lengths. It separates these games and their indexed resource editions. It is
not a cryptographic identity or a full-volume integrity check.

CSpect 3.3.1.0's folder-backed filesystem reported success for `F_MKDIR` without
creating a directory, and `F_RENAME` failed in the diagnostic probe. The
launcher therefore runs `tools/prepare_saves.ps1` to create the matching host
folder. The interpreter also requests directory creation for NextZXOS use.
The alternating-file design does not require rename or deletion support.

Use `run.ps1 -Runtime` rather than launching the NEX directly on a fresh CSpect
folder. For a manual launch, first run:

```powershell
.\tools\prepare_saves.ps1 -GameDirectory games/sq1
```

## Snapshot contents and lifecycle

Format 1 is a ZXQuest format, **not compatible with Sierra DOS save files**
(including the supplied PQSG files). It has a 60-byte little-endian header:
magic `ZXQSAVE1`, version, runtime-state size, payload length, game fingerprint,
generation, CRC32 and a 32-byte terminated description. CRC covers the header
except its CRC field, followed by the complete payload.

The current file is 78,485 bytes: 60-byte header and 78,425-byte payload.
Payload consists of the VM's 256 variables, 256 flags and 256 scan starts;
the pointer-free 1,849-byte runtime state; all 256 actors; permanent text and
attributes; and the complete packed picture/composite and clean background.
This preserves overlay pictures and permanent `add.to.pic` stamps, including
their collision/priority pixels. Inventory locations, strings, script key and
menu bindings, timing and automatic actor motion are retained. The random-state
field remains in the payload for file compatibility, but restoring deliberately
replaces it with the advanced live generator mixed with the current frame clock.
Restoring the same save therefore does not rewind the upcoming gambling spins.

Handles, open files, code pointers, the executing VM stack, parser caches,
modal backing and interrupt audio buffers are not serialized. Update the format
version when the serialized layouts or their interpretation change, even if
`sizeof(rt_state)` happens to remain the same. Future development builds are
not promised to load this format indefinitely.

Restoration reserves ten temporary 8 KB resource-cache pages, evicting only
unpinned resources. The active VM and game state remain intact until the
entire save has been read into those pages and validated. There is no disk IO
after applying the staged snapshot begins. Failure to reserve memory or read
valid data leaves the current game intact. The pages are released afterward;
normal gameplay retains the full 192 KB cache and has no save-processing work.

A successful restore stops current audio and abandons the current VM call
stack. It clears transient input/controllers, sets AGI flag 12, restores the
picture/text, and starts a fresh LOGIC 0 scan. A sound active in the saved state
is completed via its original completion flag rather than resumed mid-note.
The foreground clock excludes time spent handling the modal disk operation.
These lifecycle rules follow the pinned Sarien restore implementation.

## Validation

`tools/prepare_save_tests.py` copies the supplied games and authored fixtures
into **build/save-sd**, keeping test saves separate from player saves. It clears
only saves in that diagnostic folder. Then:

```powershell
python tools/prepare_runtime_tests.py
python tools/prepare_save_tests.py
.\build.ps1 -SaveTest
.\run.ps1 -SaveTest
```

Results are in `build/save-sd/build/save-tests.txt` and
`build/save-sd/build/compatibility-tests.txt`. Tests cover state/pixel round trips,
runtime reinitialization, modal VM continuation, restore flag lifecycle, stopped
sound completion, low memory, corrupt/truncated/foreign saves, empty slots and
recovery of the previous generation after an interrupted replacement.
The integration harness exercises the unmodified SQ1, KQ1 and PQ1 opening rooms.

The desktop UI check was retried on 25 October 2026 with CSpect 3.3.1.0 at
capped 28 MHz. Window capture and mouse input worked, and automated Down keys
navigated CSpect's native Windows menu. Automated Space, Enter and Shift+1
produced no observed response from the emulated game keyboard after refocusing;
the intro continued looping. The save/restore dialog visual round trip therefore
remains unverified through desktop automation. No test save was written, and
the existing player slot 1 retained its SHA-256 checksum. The 30 save checks
and three-game integration results above remain separate automated evidence.

The subsequent user check confirmed slot selection, saving and restoration to
the saved SQ1 location. It also exposed an unnecessary third Enter after a
successful save: the completion notice remained open. The revised UI closes
immediately after the confirmed save has been written and verified; restore
and error-dialog behavior are unchanged. All 33 save checks pass, including
exact framebuffer restoration on the second Enter and resumption without
another key. SQ1, KQ1 and PQ1 menu-controller save/restore checks also pass.

Physical Next storage, SD removal/power-failure behavior and OS bank ownership
still require hardware validation. The saves use 8.3-compatible names.

## Slot-list update

Save and Restore list descriptions with a dash and no visible slot numbers.
Up/Down repaints only the old and new selection-marker cells; it does not
restore the background or redraw the panel. The cached labels and existing
slot identities/files are unchanged. Number keys remain an undocumented
compatibility shortcut. Captured frame comparisons show ten changed pixels
for a single downward move, all within the two marker cells, for both lists.


The confirmation panels also identify the selected save only by its description;
no slot number or internal filename is displayed. Their height is reduced to fit.

Text visibility uses the previously unused tail of clean-picture page 39, which
is already part of the v1 payload. A marker distinguishes older snapshots, whose
text starts fully visible. Masks preserve partial sprite-overwritten glyphs on
restore. The derived cel-cache tail is zeroed when saving and invalidated when
restoring; session-local resource handles cannot be reused from a save file.
