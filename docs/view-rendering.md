# VIEW rendering milestone

The viewer renders Roger and other original VIEW cels directly from the
separate game resources. It does not execute scripts or implement collision.
The demonstration defaults to VIEW 1, loop 0, cel 0 at x=76, baseline y=140
over picture 1. These choices belong to the temporary viewer shell, not the
resource decoder or future engine.

## Decoder and source contract

`src/agi/view.c` uses an offset-based reader supplied by its caller. It has no
esxDOS calls, game IDs or flat resource pointer assumptions. The Next adapter
acquires and pins a VIEW cache entry, exposes bounded copies and releases its
pin on close. Keep an open `next_view` at a stable resident address because
its source context points to its own handle. Close before reopening the same
object. Reader contexts, callback code and output buffers must stay resident.

Header parsing checks the loop/cel tables and offsets before addition, so a
relative 16-bit offset cannot wrap into the header. Width and height must be
nonzero. The decoder uses a 64-byte source window and a 255-byte row buffer;
it never expands a whole VIEW into resident RAM. RLE row endings imply a
transparent tail. Mirroring reverses the expanded row only when the cel's
mirror bit is set and the requested loop differs from its original-loop bits.
Shared source bytes remain immutable.

The row callback can collect pixels, compute checksums or draw. Decode errors
can follow earlier row callbacks; consumers needing atomic drawing must first
validate the stream. `agi_view_blit` does that validation before compositing.
Its source must stay immutable and pinned across both passes. Workspace is
shared; decoder calls are not reentrant and must not be made from an ISR or
recursively from a row callback. Descriptions are located and bounds-checked,
but inventory text display is future work.

## Compositing and display

Coordinates follow AGI's lower-left convention: x is the left edge and y is
the baseline (last row), so the top is `y - height + 1`. Screen edges are clipped
without moving the cel's contents. Transparent pixels leave both nibbles
untouched. Opaque pixels replace visual and priority when their priority is
at least the current screen priority. Equal priority therefore gives the
later draw precedence. The combined runtime orders actors by priority and
baseline before calling this compositor; see [runtime.md](runtime.md).

Priority values 0–2 are control pixels. Drawing scans down the same column
for the effective priority, preserves the control nibble, and changes only
visual colour if visible. At the bottom of an all-control column, effective
priority defaults to 4 as in the evaluated Sarien implementation. Priority 3
(water) participates in normal drawing priority. Ordinary depth defaults to
`max(4, 1 + y/12)`, capped at 14. Full Sierra interpreter/version compatibility
is not claimed from these compositor rules alone.

The row compositor compares packed bytes against `(priority << 4) | 15` and
advances pointers, avoiding repeated per-pixel shifts and array indexing.
This retains inclusive priority comparisons, transparent pixels and control
nibbles. There is no retained priority cache between pixels, rows or actors.
The independent test renderer uses direct pixel reads and ordinary priority
values to check this path across narrow/wide, mirrored and clipped cels.

Partial display uploads stage at most 16 packed pixels, matching the AGI width
of an 8 KB Layer 2 bank. Separate Z80 visual/priority loops extract the selected
nibble and duplicate it into adjacent screen columns. They preserve IX/IY and
MMU mappings; the C wrapper restores the shared display bank. The fragment
count is always 1–16, and shared scratch is not used by interrupt handlers.
Full-picture and variable-width tests compare all displayed pixels at shifted
origins, including row 255. See [performance.md](performance.md) for timings.

The clean packed background lives in MMU pages 36–39. The composite uses
pages 32–35. Changing a sprite restores its old rectangle, draws the new cel,
then uploads both rectangles from the final composite to Layer 2. Transparent
pixels, changes in cel size and overlapping old/new rectangles therefore do
not leave trails. Snapshot, restore and rectangle upload preserve MMU3; MMU2
remains owned by the pixel-access layer. Rectangle endpoints use 32-bit
arithmetic before clipping, including for extreme caller coordinates.

The viewer moves one pixel per step, selects conventional walking directions
for 2–4-loop views and cycles cels. Held movement uses a temporary busy delay
calibrated to the installed 3.5 MHz z88dk delay routine while the CPU runs at
28 MHz (about 40 ms plus drawing). This is diagnostic pacing, not AGI timing.
Other controls act once per keypress. N/B still reload and decode a picture;
the new rectangle path speeds sprite updates, not vector picture decoding.

## Validation approach

`tools/prepare_view_tests.py` is a separate Python decoder based on the VIEW
format. It writes each cel's dimensions, transparency, orientation and decoded
CRC-16 into an external test manifest. The target decoder checks every cel in
CSpect, including shared/mirrored loops. Synthetic tests cover malformed data,
8 KB source boundaries, clipping, control/priority pixels, overlapping sprites,
background restoration, partial display uploads and unchanged HUD pixels.
The test also exports a real room composited with Roger. No generated image
or checksum manifest is used by the normal viewer.

Sources consulted:

* [AGI VIEW resource format, Peter Kelly and Claudio Matsuoka](https://www.agidev.com/articles/agispec/agispecs-8.html)
* [Sarien VIEW metadata](https://github.com/cmatsuoka/sarien/blob/aaef5e667757385b5b3650cc1f7365908720f7e6/src/view.c)
* [Sarien sprite priority handling](https://github.com/cmatsuoka/sarien/blob/aaef5e667757385b5b3650cc1f7365908720f7e6/src/sprite.c)
* [ScummVM AGI sprite rendering](https://github.com/scummvm/scummvm/blob/master/engines/agi/sprite.cpp)

The new reader, decoder and banked drawing glue are project code; the upstream
VM remains unlinked. No additional original game assets are distributed.


## Display ordering and cached cels - 9 November 2026

Display/clear-lines flush pending actor drawing before emitting text, preserving
script order. Subsequent changed actors erase text only within their old/new
rectangles. Four visibility bytes per text cell retain the surviving doubled
pixel pairs, so union-rectangle repairs and full redraws cannot revive covered
dots or overwrite nearby labels. Authored blank padding follows the same rule.
A row-presence bitmap skips empty text rows; show.pic clears the corresponding
row metadata along with its cells.

Page 39's unused tail stores masks at $6900-$789f, marker/row bits at $78a0,
and a derived 32-entry cel cache at $7900 (marker $7ff0). Picture capture copies
only the 26,880 actual picture bytes and leaves this tail intact. Snapshots keep
the visibility metadata, initialize it for old saves, and zero/invalidate the
session-local cel cache. The save layout and version remain unchanged.

The cel cache is enabled only for immutable Next resource readers. Complete
resource generation, length, loop and cel identify metadata; full cel metadata
and generation identify successful RLE validation. Failed/truncated decodes
are never cached. Other readers still validate every blit. VIEW and compositor
code share page 93; their internal calls avoid resident bank gates.

Composition retains actor ordering and whole-actor overlap expansion, but skips
empty vertical bands between affected actors. Every band edge follows control
priority runs before painting. Sparse actor scans skip empty groups of eight
IDs while retaining occupied-slot order, including actor 255.

Run tools/prepare_display_tests.py, then tools/build_banked.ps1 -TestDisplay for
the original SQ1 keypad/payout scenarios, legacy text metadata, pixel-accurate
partial occlusion and malformed/warm-cache checks. The parser test variant also
runs dirty/full-render comparisons and measured corridor workloads.
