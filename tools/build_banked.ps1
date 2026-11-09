param([string]$Z88dkRoot='C:\z88dk', [switch]$TestVM, [switch]$TestRuntime, [switch]$TestParser, [switch]$TestMotion, [switch]$TestInventory, [switch]$TestSound, [switch]$TestSave, [switch]$TestUI, [switch]$TestCloset, [switch]$TestControls, [switch]$TestKerona, [switch]$TestDisplay)
$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    if($TestKerona -or $TestDisplay){$TestControls=$true}
    $env:PATH="$Z88dkRoot\bin;$env:PATH"
    $env:ZCCCFG="$Z88dkRoot\lib\config\"
    $variant=if($TestControls){'control-tests'}elseif($TestCloset){'closet-tests'}elseif($TestUI){'ui-tests'}elseif($TestSave){'save-tests'}elseif($TestSound){'sound-tests'}elseif($TestInventory){'inventory-tests'}elseif($TestMotion){'motion-tests'}elseif($TestParser){'parser-tests'}elseif($TestVM){'banked-logic-tests'}elseif($TestRuntime){'runtime-tests'}else{'runtime'}
    $objectDir=if($TestRuntime -or $TestParser -or $TestMotion -or $TestInventory -or $TestSound -or $TestSave -or $TestUI -or $TestCloset -or $TestControls){'build/obj-runtime'}else{"build/obj-$variant"}
    New-Item -ItemType Directory -Force $objectDir | Out-Null
    if($TestVM -and !(Test-Path 'build/logic_manifest.h')){throw 'Run python tools/prepare_logic_tests.py first.'}
    if($TestRuntime -and !(Test-Path 'build/runtime_manifest.h')){throw 'Run python tools/prepare_runtime_tests.py first.'}
    if($TestInventory -and !(Test-Path 'build/inventory-fixtures/OBJECT')){throw 'Run python tools/prepare_inventory_tests.py first.'}
    if($TestSound -and (!(Test-Path 'build/runtime_manifest.h') -or !(Test-Path 'build/sound-fixtures/SNDDIR'))){throw 'Run python tools/prepare_runtime_tests.py and python tools/prepare_sound_tests.py first.'}
    $base=@('+zxn','-clib=sdcc_iy','-SO3','--max-allocs-per-node20000','-Isrc')
    if($TestVM){$base+='-DBANKED_VM_ONLY'}
    $vmNames=@('agi_vm_init','agi_vm_stop','agi_vm_reset_scans','agi_vm_start','agi_vm_run','agi_vm_resume','agi_vm_flag','agi_vm_set_flag','agi_vm_read_code','agi_logic_header','agi_logic_message')
    $viewNames=@('agi_view_inspect','agi_view_select','agi_view_decode','agi_view_blit','agi_view_priority')
    $main=if($TestControls){'src/ui_test_main.c'}elseif($TestCloset){'src/closet_test_main.c'}elseif($TestUI){'src/ui_test_main.c'}elseif($TestSave){'src/save_test_main.c'}elseif($TestSound){'src/sound_test_main.c'}elseif($TestInventory){'src/inventory_test_main.c'}elseif($TestMotion){'src/motion_test_main.c'}elseif($TestParser){'src/parser_test_main.c'}elseif($TestVM){'tests/logic_tests.c'}elseif($TestRuntime){'src/runtime_test_main.c'}else{'src/runtime_entry.c'}
    $units=@(
        @{file=$main;page=0},
        @{file='src/next/platform.c';page=0},
        @{file='src/next/resource.c';page=53;rename=@('resource_open','resource_read','resource_close','resource_load')},
        @{file='src/next/resource_cache_load.c';page=53;rename=@('resource_cache_init','resource_acquire','resource_scratch','resource_scratch_release')},
        @{file='src/next/resource_cache.c';page=0},
        @{file='src/next/logic_resource.c';page=0},
        @{file='src/next/logic_preload.c';page=53;rename=@('next_logic_command')},
        @{file='src/next/resource_cache_status.c';page=53;rename=@('resource_cache_status')},
        @{file='src/next/bank_calls.c';page=0},
        @{file='src/agi/logic.c';page=50;rename=$vmNames},
        @{file='src/agi/opcodes.c';page=50}
    )
    if(!$TestVM -and !$TestRuntime -and !$TestParser -and !$TestMotion -and !$TestInventory -and !$TestSound -and !$TestSave -and !$TestUI -and !$TestCloset -and !$TestControls){
        $units+=@(@{file='src/runtime_main.c';page=52;rename=@('main=rt_main')})
    }
    if(!$TestVM){
        $units+=@(
            @{file='src/agi/picture.c';page=51;rename=@('agi_picture_decode')},
            @{file='src/agi/view.c';page=93;rename=$viewNames},
            @{file='src/next/view_resource.c';page=0},
            @{file='src/next/view_setup.c';page=53;rename=@('next_view_open','next_view_close')},
            @{file='src/next/scene.c';page=52;rename=@('next_scene_capture','next_scene_restore','next_show_rect')},
            @{file='src/next/runtime_state.c';page=53;rename=@('rt_actor_read','rt_actor_write','rt_picture','rt_redraw')},
            @{file='src/next/runtime_display.c';page=55;rename=@('next_screen_clear','next_agi_char','next_menu_restore')},
            @{file='src/next/clock.c';page=0},
            @{file='src/next/runtime_platform.c';page=52;rename=@('next_init','next_show_picture')},
            @{file='src/next/runtime_frontend.c';page=55;rename=@('next_text')},
            @{file='src/next/runtime_clock_start.c';page=52;rename=@('next_clock_start','next_key_decode','next_key_reset')},
            @{file='src/next/sound.c';page=0},
            @{file='src/next/sound_begin.c';page=56;rename=@('next_sound_begin')},
            @{file='src/next/runtime_sound.c';page=56;rename=@('rt_sound_start','rt_sound_stop','rt_sound_poll')},
            @{file='src/next/runtime.c';page=53;rename=@('rt_init','rt_step','rt_key','rt_command','rt_test','rt_host_key','rt_stop_message')},
            @{file='src/next/runtime_actor.c';page=54;rename=@('rt_actor_command','rt_actor_tick','rt_actor_reset','rt_actor_test')},
            @{file='src/next/runtime_collision.c';page=54},
            @{file='src/next/runtime_inventory.c';page=95;rename=@('rt_objects','rt_object_name','rt_inventory')},
            @{file='src/next/runtime_motion.c';page=95;rename=@('rt_motion_direction')},
            @{file='src/next/runtime_render.c';page=93;rename=@('rt_actor_render','rt_shake')},
            @{file='src/next/runtime_text_mask.c';page=56;rename=@('rt_text_mask')},
            @{file='src/next/runtime_save.c';page=92;rename=@('rt_save_slot','rt_save_description')},
            @{file='src/next/runtime_save_ui.c';page=51;rename=@('rt_save_ui')},
            @{file='src/next/runtime_startup.c';page=51;rename=@('rt_startup')},
            @{file='src/next/runtime_dialog.c';page=52;rename=@('rt_dialog')},
            @{file='src/next/runtime_frame.c';page=52;rename=@('rt_frame')},
            @{file='src/next/runtime_menu.c';page=52;rename=@('rt_menu_draw')},
            @{file='src/next/runtime_key_labels.c';page=51;rename=@('rt_key_labels')},
            @{file='src/next/runtime_controls.c';page=95;rename=@('rt_control','rt_control_apply')},
            @{file='src/next/runtime_joystick.c';page=51;rename=@('rt_joystick','rt_joystick_config')},
            @{file='src/next/runtime_text.c';page=55;rename=@('rt_text_command','rt_format','rt_ui_key','rt_text_clear','rt_text_redraw','rt_text_at','rt_modal')}
            @{file='src/next/runtime_parser.c';page=56;rename=@('rt_parser_init','rt_parser_command','rt_parser_test','rt_parser_key','rt_parser_cycle','rt_parser_word','rt_parser_reset_input','rt_parser_invalidate_display')}
        )
    }
    if($TestControls){$testFile=if($TestDisplay){'tests/display_tests.c'}elseif($TestKerona){'tests/kerona_tests.c'}else{'tests/control_tests.c'};$units+=@(@{file=$testFile;page=88;rename=@('main=ui_tests_main')})}
    if($TestCloset){$units+=@(@{file='tests/closet_tests.c';page=88;rename=@('main=ui_tests_main')},@{file='tests/motion_tests.c';page=90})}
    if($TestUI){$units+=@(@{file='tests/ui_tests.c';page=88;rename=@('main=ui_tests_main')})}
    if($TestRuntime){$units+=@(
        @{file='tests/runtime_tests.c';page=88;rename=@('main=runtime_tests_main')},
        @{file='tests/runtime_feedback_tests.c';page=89},
        @{file='tests/parser_tests.c';page=90}
    )}
    if($TestParser){$units+=@(@{file='tests/parser_tests.c';page=90})}
    if($TestRuntime -or $TestParser){$units+=@(@{file='tests/render_tests.c';page=57})}
    if($TestMotion){$units+=@(@{file='tests/motion_tests.c';page=90},@{file='tests/room_tests.c';page=94},@{file='tests/follow_tests.c';page=57})}
    if($TestInventory){$units+=@(@{file='tests/inventory_tests.c';page=57},@{file='tests/inventory_game_tests.c';page=89})}
    if($TestSound){$units+=@(@{file='tests/sound_tests.c';page=57},@{file='tests/sound_game_tests.c';page=89})}
    if($TestSave){
        if(!(Test-Path 'build/save_manifest.h')){throw 'Run tools/prepare_save_tests.py first.'}
        $units+=@(@{file='tests/save_tests.c';page=88},@{file='tests/compatibility_tests.c';page=89})
    }
    $headers=Get-ChildItem src -Filter '*.h' -Recurse
    if($TestVM){$headers+=Get-Item 'build/logic_manifest.h'}
    if($TestSound){$headers+=Get-Item 'build/runtime_manifest.h'}
    $objects=@();$banks=@()
    foreach($unit in $units){
        $obj=Join-Path $objectDir ($unit.file.Replace('/','_').Replace('.c','.o'))
        $argsForUnit=$base+@('-c',$unit.file,'-o',$obj)
        if($unit.file -eq 'src/agi/view.c'){$argsForUnit+='-DVIEW_RESOURCE_CACHE'}
        if($unit.file -eq 'src/next/platform.c' -and !$TestVM){$argsForUnit+='-DBANKED_PLATFORM'}
        if($unit.page){
            $bank=[int][Math]::Floor($unit.page/2)
            $half=if($unit.page%2){'H'}else{'L'}
            $section="BANK_${bank}_$half"
            $argsForUnit+=@("--codeseg$section","--constseg$section")
            $banks+=@{section=$section;page=$unit.page}
        }
        foreach($symbol in $unit.rename){
            if($symbol.Contains('=')){$pair=$symbol.Split('=');$argsForUnit+="-D$($pair[0])=bank_$($pair[1])"}
            else{$argsForUnit+="-D$symbol=bank_$symbol"}
        }
        $fingerprint=$argsForUnit -join "`n"
        $needs=!(Test-Path $obj) -or !(Test-Path "$obj.args")
        if(!$needs){
            $stamp=(Get-Item $obj).LastWriteTimeUtc
            $needs=(Get-Item $unit.file).LastWriteTimeUtc -gt $stamp -or
                @($headers | Where-Object LastWriteTimeUtc -gt $stamp).Count -gt 0 -or
                (Get-Content "$obj.args" -Raw) -cne $fingerprint
            if($unit.file -eq 'tests/runtime_tests.c'){$needs=$needs -or (Get-Item 'build/runtime_manifest.h').LastWriteTimeUtc -gt $stamp}
            if($unit.file -eq 'tests/save_tests.c'){$needs=$needs -or (Get-Item 'build/save_manifest.h').LastWriteTimeUtc -gt $stamp}
        }
        if($needs){
            Write-Host "Compiling $($unit.file)"
            & "$Z88dkRoot\bin\zcc.exe" @argsForUnit
            if($LASTEXITCODE -ne 0){throw "Compile failed: $($unit.file)"}
            [IO.File]::WriteAllText((Join-Path (Get-Location) "$obj.args"),$fingerprint)
        }
        $objects+=$obj
    }
    if($TestKerona){$variant='kerona-tests'}
    if($TestDisplay){$variant='display-tests'}
    $output="build/$variant"
    $link=$base+@('-subtype=nex','-startup=0','-pragma-define:CRT_ORG_CODE=40960','-pragma-define:REGISTER_SP=65280','-pragma-define:CRT_STACK_SIZE=1024','-pragma-define:CRT_ENABLE_EIDI=0')
    foreach($bank in $banks | ForEach-Object { [pscustomobject]$_ } | Sort-Object page -Unique){$link+="-pragma-define:CRT_ORG_$($bank.section)=32768"}
    $link+=$objects+@('-o',$output,'-m','-create-app')
    & "$Z88dkRoot\bin\zcc.exe" @link
    if($LASTEXITCODE -ne 0){throw 'Banked link failed'}
    $map=Get-Content "$output.map" -Raw
    $m=[regex]::Match($map,'(?m)^__BSS_END_tail\s+= \$([0-9A-Fa-f]+)')
    # The VM-only diagnostic has interrupts disabled and installs no IM2 table.
    # Runtime variants reserve $f800; every variant reserves the $fb00 stack.
    $residentLimit=if($TestVM){0xFB00}else{0xF800}
    if(!$m.Success -or [Convert]::ToInt32($m.Groups[1].Value,16) -gt $residentLimit){throw 'Resident image overlaps reserved IM2/stack space'}
    $residentFree=$residentLimit-[Convert]::ToInt32($m.Groups[1].Value,16)
    if($residentFree -lt 256){Write-Warning "Resident headroom is only $residentFree bytes."}
    foreach($bank in $banks | ForEach-Object { [pscustomobject]$_ } | Sort-Object page -Unique){
        $s=[regex]::Match($map,"(?m)^__$($bank.section)_size\s+= \`$([0-9A-Fa-f]+)")
        if(!$s.Success -or [Convert]::ToInt32($s.Groups[1].Value,16) -gt 8192){throw "Overlay overflow: $($bank.section)"}
        $bankUsed=[Convert]::ToInt32($s.Groups[1].Value,16)
        $bankFree=8192-$bankUsed
        Write-Host "Page $($bank.page): $bankUsed used, $bankFree free bytes"
        if($bankFree -lt 512){Write-Warning "Page $($bank.page) has less than 512 bytes of headroom."}
    }
    $header=[IO.File]::ReadAllBytes((Join-Path (Get-Location) "$output.nex"))
    if([Text.Encoding]::ASCII.GetString($header,0,4) -ne 'Next' -or $header[8] -ne 0){throw 'Expected base 1MB NEX'}
    Write-Host "Resident end: `$$($m.Groups[1].Value); $residentFree bytes before reserved IM2/stack. Built $output.nex"
} finally { Pop-Location }
