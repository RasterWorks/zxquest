param([string]$Source = 'build/upstream/sarien', [string]$Z88dkRoot = 'C:\z88dk')
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    $expected = 'aaef5e667757385b5b3650cc1f7365908720f7e6'
    $revision = & git -C $Source rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $revision -ne $expected) { throw "Expected Sarien revision $expected" }
    $probe = 'build/sarien-probe'
    New-Item -ItemType Directory -Force $probe | Out-Null
    Copy-Item "$Source/src/*.h" $probe
    foreach ($unit in @('logic', 'op_cmd', 'op_test', 'motion')) {
        Copy-Item "$Source/src/$unit.c" $probe
    }
    $port = Get-Content "$probe/portdefs.h" -Raw
    $port = $port -replace 'unsigned int\s+UINT32', 'unsigned long UINT32'
    $port = $port -replace 'signed int\s+SINT32', 'signed long SINT32'
    $port += "`n#undef USE_HIRES`n#undef USE_PCM_SOUND`n#undef USE_IIGS_SOUND`n#undef USE_CONSOLE`n#undef USE_MOUSE`n#undef USE_COMMAND_LINE`n#define VERSION `"0.8-zxn-probe`"`n"
    Set-Content "$probe/portdefs.h" $port -Encoding ascii
    @'
#include "sarien.h"
#include "agi.h"
unsigned int probe_game_size(void) { return sizeof(struct agi_game); }
unsigned int probe_dir_size(void) { return sizeof(struct agi_dir); }
unsigned int probe_actor_size(void) { return sizeof(struct vt_entry); }
unsigned int probe_uint32_size(void) { return sizeof(UINT32); }
'@ | Set-Content "$probe/sizes.c" -Encoding ascii
    $env:PATH = "$Z88dkRoot\bin;$env:PATH"
    $env:ZCCCFG = "$Z88dkRoot\lib\config\"
    foreach ($unit in @('sizes', 'logic', 'op_cmd', 'op_test', 'motion')) {
        & "$Z88dkRoot/bin/zcc.exe" '+zxn' '-clib=sdcc_iy' '-S' '-O2' "-I$probe" "$probe/$unit.c" '-o' "$probe/$unit.asm"
        if ($LASTEXITCODE -ne 0) { throw "Sarien compile probe failed: $unit" }
        & "$Z88dkRoot/bin/zcc.exe" '+zxn' '-clib=sdcc_iy' '-c' "$probe/$unit.asm" '-o' "$probe/$unit.o"
        if ($LASTEXITCODE -ne 0) { throw "Sarien assembly probe failed: $unit" }
        Write-Host "Compiled $unit to a Z80 object (not linked into an engine)."
        & "$Z88dkRoot/bin/z88dk-z80nm.exe" "$probe/$unit.o" | Select-String 'Section (code_compiler|rodata_compiler|data_compiler|bss_compiler)'
    }
} finally { Pop-Location }
