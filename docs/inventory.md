# Inventory and item inspection

`status` (opcode 124) lists the objects whose current location is 255, using
names read from the separate OBJECT file. Plain and Avis Durgan encrypted
DOS tables are supported, with up to 256 entries. The interpreter validates
table bounds, name offsets and string terminators. It does not embed SQ1's
item names or item-to-VIEW mapping.

Without AGI flag 13, a key closes the list and leaves variable 25 unchanged.
With flag 13 set, the arrow keys select an item; Enter writes its OBJECT ID
to variable 25, and cancel writes 255. Empty selection also returns 255.
Selection starts at the first carried item. The two-column list shows 40
items per page; down/up cross pages automatically. Ordinary, nonselecting
lists with more than 40 items also accept up/down to change pages. Object
255 shares the original AGI cancellation value and is not distinguishable
from cancellation through v25.

`show.obj` (129) and `show.obj.v` (162) display loop 0, cel 0 of the requested
VIEW and its description. The argument is a VIEW number, not an inventory
ID. Description substitutions use the same formatter as LOGIC messages,
including live variables, strings, nested messages and `%n` object names.
Text wraps at word boundaries and continues onto additional pages. Enter
advances text or closes the final page; Shift+1 cancels immediately. No game
script continues until the inspection closes.

In CSpect, use **Shift+1** for the game menu and cancellation, and
**Shift+5/6/7/8** for left/down/up/right. **Enter** confirms. Host Escape
closes the emulator, so use Shift+1 to cancel within the game.

SQ1 decides which VIEW to inspect and what to do afterward. In particular,
its original survival-kit branch displays the kit, waits for dismissal,
then replaces the closed kit with the knife, dehydrated water and opened
kit. The interpreter implements these opcodes without special-casing those
items. The integration test stages possessions to exercise this branch;
it is not a playthrough to obtaining the kit.

## Display and memory

Inventory and inspection capture the 320x200 viewport in the existing
32 KB modal backing pages and restore those pixels on dismissal. They do
not reconstruct the room or mutate actors, the priority map or persistent
text. Top/bottom display margins remain untouched. Navigation within a page
updates only the old/new selection and the full-name footer. Normal game
scans do not perform inventory name reads or inventory drawing.

Names are read on demand; the table's offsets and path use page 61 from
`$6c00`. A compile-time check bounds the parser state below that address.
Formatting scratch moved out of resident RAM: `$7e00..$7fff` holds the raw
message, and `$7000..$72ff` retains modal text while the resident shared
buffer stages captured pixels. Resource reads still stage through resident
buffers, and every shared data-bank mapping is restored. Code shares page
95 with follow/wander decisions; text and backing helpers remain in page 55.

Current UI bounds: list columns display 19 characters, with the selected
name repeated in a 40-character footer. Names longer than 40 characters are
validated to their terminator but clipped for display/substitution. Raw
VIEW descriptions allow 511 characters, expanded text 767. The item-image
panel is 320x104 pixels; oversized cels are clipped without touching text or
outside margins. All supplied SQ1 inventory cels fit this panel. These are
explicit frontend limits for subsequent games, not modifications to assets.

## Regression suite

```powershell
python tools/prepare_inventory_tests.py
.\build.ps1 -InventoryTest
.\run.ps1 -InventoryTest
```

The authored suite writes `build/inventory-tests.txt`; the separate original
game integration suite writes `build/inventory-game-tests.txt`. Both must
end in `SUMMARY ... failures=0`. Tests cover encrypted/plain/empty/full
OBJECT tables, navigation, malformed resources, VM wait/resume and pin
lifetimes, live substitutions, cel transparency/mirroring, description
pagination, bank preservation, stack bounds and framebuffer restoration.
Frame dumps under `build/inventory-*.l2` can be inspected with
`tools/render_dump.py --layer2`.

Selection semantics derive from the pinned Sarien `src/inv.c`, and the
inspection contract from `src/sprite.c` and `src/op_cmd.c`; see
[provenance](interpreter-foundation.md) and [COPYING](../COPYING).
