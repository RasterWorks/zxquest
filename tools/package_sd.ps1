param(
    [string]$OutputDirectory='build/sd-card',
    [ValidateSet('sq1','kq1','pq1')][string[]]$Games=@('sq1','kq1','pq1')
)
$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    $packageRoot=[IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
    if(Test-Path -LiteralPath $packageRoot){throw "Output already exists; choose a new OutputDirectory to preserve packaged saves: $packageRoot"}
    $binary=Get-Item -LiteralPath 'build/runtime.nex'
    $header=[IO.File]::ReadAllBytes($binary.FullName)
    if([Text.Encoding]::ASCII.GetString($header,0,4) -ne 'Next'){throw 'Expected a built runtime NEX.'}
    $required=@('LOGDIR','PICDIR','VIEWDIR','SNDDIR','OBJECT','WORDS.TOK')
    foreach($game in $Games){foreach($name in $required){if(!(Test-Path -LiteralPath "games/$game/$name")){throw "Missing games/$game/$name"}}}
    $bundle=Join-Path $packageRoot 'ZXQUEST'
    New-Item -ItemType Directory -Path $bundle | Out-Null
    function Copy-Verified([string]$Source,[string]$Destination){
        Copy-Item -LiteralPath $Source -Destination $Destination
        if((Get-FileHash -LiteralPath $Source).Hash -ne (Get-FileHash -LiteralPath $Destination).Hash){throw "Copy verification failed: $Destination"}
    }
    foreach($game in $Games){
        $gameRoot=Join-Path $bundle $game.ToUpperInvariant()
        $dataRoot=Join-Path $gameRoot "games/$game"
        New-Item -ItemType Directory -Path $dataRoot -Force | Out-Null
        Copy-Verified $binary.FullName (Join-Path $gameRoot 'ZXQUEST.NEX')
        [IO.File]::WriteAllText((Join-Path $gameRoot 'ZXQUEST.CFG'),"games/$game`r`n",[Text.Encoding]::ASCII)
        foreach($name in $required){Copy-Verified "games/$game/$name" (Join-Path $dataRoot $name)}
        $volumes=Get-ChildItem -LiteralPath "games/$game" -File | Where-Object Name -Match '^VOL\.[0-9]+$'
        if(!$volumes){throw "No volumes found for $game"}
        foreach($volume in $volumes){Copy-Verified $volume.FullName (Join-Path $dataRoot $volume.Name)}
        & "$PSScriptRoot/prepare_saves.ps1" -SdRoot $gameRoot -GameDirectory "games/$game"
        $saveFolder=Get-ChildItem -LiteralPath (Join-Path $gameRoot 'saves') -Directory
        $existing=Join-Path 'saves' $saveFolder.Name
        if(Test-Path -LiteralPath $existing){
            foreach($save in Get-ChildItem -LiteralPath $existing -File | Where-Object Name -Match '^[1-9]\.[ab]$'){
                Copy-Verified $save.FullName (Join-Path $saveFolder.FullName $save.Name)
            }
        }
    }
    Copy-Verified 'COPYING' (Join-Path $bundle 'COPYING.TXT')
    $instructions=@'
ZXQUEST - ZX SPECTRUM NEXT HARDWARE TEST

INSTALL AND START
1. Copy this whole ZXQUEST folder to the SD card, keeping its subfolders.
2. In the NextZXOS Browser, enter ZXQUEST, then SQ1, KQ1 or PQ1.
3. Select ZXQUEST.NEX and press Enter.

Open the NEX from inside its game folder: resource/config/save paths are
relative to that folder. Each game has its own configuration, so no editing
or PC launch script is needed. SQ1 is the most extensively tested game.

CONTENTS OF EACH GAME FOLDER
ZXQUEST.NEX   Current banked interpreter, identical for all three games.
ZXQUEST.CFG   Selects that folder's game data.
games/...    LOGDIR, PICDIR, VIEWDIR, SNDDIR, OBJECT, WORDS.TOK and VOL.n.
saves/...    Game-specific save folder and copies of existing project saves.

The original DOS executables, graphics drivers and emulator plugins are not
needed. Keep both .a and .b save files: they are the current/previous copies
of each slot. Hardware saves stay on this SD card; they do not automatically
sync back to the PC project.

HARDWARE AND CONTROLS
Target: ZX Spectrum Next, base 1 MB, NextZXOS. The program selects 28 MHz.
Core 3.1.5 or newer supports the dedicated extended-key register path.
No Pi accelerator is required. Initial SQ1 gameplay has been tested on a real
Next; further hardware testing is ongoing.

Arrow keys           Move; navigate menus and save slots.
                     Combine two arrows for diagonals; staggered releases
                     retain direction. Press the same direction again to stop.
Break                Open/close menu; cancel a dialog.
Enter                Select, submit typed command, dismiss dialog.
Delete               Delete last typed character.
INV VIDEO            Inventory (Graph has no inventory action).
Edit + 1             Help (SQ1).
Edit + 2             Sound toggle (SQ1).
Edit + 5             Save (SQ1).
Edit + 7             Restore (SQ1).
Edit + 9             Restart (SQ1; confirmation).
Edit + Q             Quit (confirmation; resets real Next).
Edit + J             Joystick selection.
Space                Skip SQ1 title sequence.

Edit + 1..9,0 supplies F1..F10; game bindings can differ.
Extended Mode remains a compatibility alias. Edit alone has no action.
Classic fallback: Caps Shift+1 for menu, +5/6/7/8 for arrows, +0 for Delete;
Caps Shift+Symbol Shift+number supplies the function keys.

BUILD NOTES
Includes the Autonav text, cave animation/transition and wandering-edge fixes.
Includes print.at / print.at.v support for the skimmer's settlement message.
Menus and key bindings survive in-game restart; stale queued keys are cleared.
Save/Restore confirmations use descriptions without slot numbers.
Sprite/text draw order preserves keypad scrolling and clears cutscene prompts.
Validated cel caching, sparse actor scans and split redraw bands improve speed.
Includes gambling random-sequence and animated-text fixes; E4/E6/E8 are short
labels for Edit + 4/6/8. E1-E0 means hold EDIT and press 1-9,0.
The queued second-spider arrival discussed during testing has NOT been changed;
the supplied SQ1 scripts retain that behaviour. For a clean SQ1 route, restore
"On Kerona with kit and glass" and crush the droid before entering the caves.

Please note core/NextZXOS version and any loading, keyboard, audio, drawing or
save/restore problems during hardware testing.

Browser reference: https://wiki.specnext.dev/File_Formats
SHA256.TXT records the packaged file hashes before hardware testing.
'@
    [IO.File]::WriteAllText((Join-Path $bundle 'README.TXT'),$instructions,[Text.Encoding]::ASCII)
    $manifest=foreach($file in Get-ChildItem -LiteralPath $bundle -Recurse -File | Sort-Object FullName){
        $relative=$file.FullName.Substring($bundle.Length+1).Replace('\','/')
        '{0}  {1}' -f (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant(),$relative
    }
    [IO.File]::WriteAllLines((Join-Path $bundle 'SHA256.TXT'),$manifest,[Text.Encoding]::ASCII)
    Write-Host "Ready to copy: $bundle"
} finally {Pop-Location}
