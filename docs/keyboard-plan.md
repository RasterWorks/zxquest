# Spectrum Next keyboard

## Controls update — 9 November 2026

| Next key | Purpose |
| --- | --- |
| Break | Menu / Cancel; the open menu pauses gameplay |
| INV VIDEO | Inventory (the game's original Tab binding) |
| Hold Edit + 1–9, 0 | Sierra F1–F10, labelled E1–E9 and E0 |
| Edit + Q / J | Quit / Joystick, labelled E-Q / E-J |
| Dedicated arrows | Walk in eight directions; navigate menus and save slots |
| Delete | Delete the previous typed character |
| Enter | Select, submit a command, or dismiss a dialogue |

E5 means hold EDIT and press 5. Edit alone is only a modifier: it neither opens
Inventory nor arms a later digit. Releasing Edit before its digit does not leak
the digit into the command line. Break overrides an outstanding function key.

Walking retains press-to-walk / press-the-same-direction-again-to-stop.
Press two perpendicular arrows for a diagonal. Adding the second arrow changes
direction without toggling movement off. Releasing the two keys at slightly
different times retains the chosen diagonal; release all arrows before choosing
a new direction. Opposing directions cancel their axis (Left+Right stops;
Left+Right+Down selects Down). Menu and slot navigation use cardinal arrows.

The user confirmed Break/Menu and Edit/Inventory on their updated core before
this mapping change. This revised mapping still needs physical hardware testing.
Dedicated-key support uses core 3.1.5 or later. Older cores cannot reliably
distinguish dedicated Edit from its Caps Shift+1 matrix equivalent.

## Restart lifecycle

Restart retains key bindings and submitted menu definitions, re-enables items,
and sets AGI flag 6. Original SQ1 LOGIC 0 skips menu construction when that flag
is set; clearing the definitions caused Break to stop opening the menu.
Keyboard reset flushes queued events, clears shortcut/arrow state and waits for
held keys to be released. Sound preference is retained. No save layout changes.

## Compatibility controls and CSpect

Extended Mode and Caps Shift+Symbol Shift remain function-key aliases. Graph
has no Inventory action. Caps Shift+1, +5/6/7/8 and +0 retain the classic
menu, cardinal-direction and deletion controls. Symbol Shift punctuation works.

CSpect starts with -basickeys -esc: PC arrows navigate and Escape opens/cancels
the game menu. Tap PC Tab, then a digit for F1–F10 (Tab,5 Save; Tab,7 Restore),
or Q/J for Quit/Joystick. This emulator prefix remains a compatibility path;
Next-facing labels always describe Edit. Inventory is also available through
the game's Action menu. Physical diagonal chords are tested via the Next's
independent arrow register; CSpect's classic synthesized arrows may not expose
simultaneous arrows through that register.

The launcher stages CSpect and its DLLs in build/cspect and compiles the local
keyboard bridge. The bridge retains a Tab event until NextReg B1 is read;
the decoder then keeps a one-shot prefix. The installed emulator is unchanged.
The bridge log is build/cspect/zxquest-keyboard.log.

## Interrupt and hardware handling

The 50 Hz interrupt samples keys, queues compact events and advances sound.
The decoder lives in code bank 52, entered through a resident gate that restores
MMU4. It uses resident state, touches no resource buffers or filesystem, and
preserves the caller's data mappings. The interrupt restores the NextReg
selector. AGI dispatch runs in the foreground.

On real Next core 3.1.5 or newer, startup sets NextReg 0x68 bit 4 while preserving
its other bits, suppressing synthesized matrix duplicates. B0 supplies arrows
and punctuation; B1 supplies dedicated modifier and action keys. Unsupported
register values of 0xff use the classic matrix fallback.

Quit asks for confirmation, stops the VM/audio and resets the real Next.
CSpect instead displays GAME ENDED. Pause freezes the game clock and AY stream;
its dismissal key is consumed. Joystick selection supports keyboard-only or
Kempston ports 1/2, eight directions, release-to-stop, Fire=Enter and Fire2=Cancel.
Connector settings are session preferences, not saved game state.

Reference: [Next register specification](https://gitlab.com/SpectrumNext/ZX_Spectrum_Next_FPGA/-/raw/master/cores/zxnext/nextreg.txt).
See [validation notes](validation.md) for test results and hardware reports.

Graph no longer opens Inventory (9 November follow-up). The save regression
suite explicitly checks that Graph emits no inventory event.
