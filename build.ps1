param([string]$Z88dkRoot = 'C:\z88dk', [switch]$Test, [switch]$Benchmark, [switch]$ResourceTest, [switch]$ViewTest, [switch]$LogicTest, [switch]$Logic, [switch]$BankedLogicTest, [switch]$Runtime, [switch]$RuntimeTest, [switch]$ParserTest, [switch]$MotionTest, [switch]$InventoryTest, [switch]$SoundTest, [switch]$SaveTest)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $env:PATH = "$Z88dkRoot\bin;$env:PATH"
    $env:ZCCCFG = "$Z88dkRoot\lib\config\"
    New-Item -ItemType Directory -Force -Path build | Out-Null
    if (([int]$Test.IsPresent + [int]$Benchmark.IsPresent + [int]$ResourceTest.IsPresent + [int]$ViewTest.IsPresent + [int]$LogicTest.IsPresent + [int]$Logic.IsPresent + [int]$BankedLogicTest.IsPresent + [int]$Runtime.IsPresent + [int]$RuntimeTest.IsPresent + [int]$ParserTest.IsPresent + [int]$MotionTest.IsPresent + [int]$InventoryTest.IsPresent + [int]$SoundTest.IsPresent + [int]$SaveTest.IsPresent) -gt 1) { throw 'Choose one build mode.' }
    if ($BankedLogicTest -or $Runtime -or $RuntimeTest -or $ParserTest -or $MotionTest -or $InventoryTest -or $SoundTest -or $SaveTest) {
        & "$PSScriptRoot\tools\build_banked.ps1" -Z88dkRoot $Z88dkRoot -TestVM:$BankedLogicTest -TestRuntime:$RuntimeTest -TestParser:$ParserTest -TestMotion:$MotionTest -TestInventory:$InventoryTest -TestSound:$SoundTest -TestSave:$SaveTest
        return
    }
    if ($ResourceTest -and !(Test-Path 'build/resource_manifest.h')) { throw 'Run python tools/prepare_resource_tests.py first.' }
    if ($ViewTest -and !(Test-Path 'build/view_manifest.h')) { throw 'Run python tools/prepare_view_tests.py first.' }
    if ($LogicTest -and !(Test-Path 'build/logic_manifest.h')) { throw 'Run python tools/prepare_logic_tests.py first.' }
    $main = if ($LogicTest) { 'tests/logic_tests.c' } elseif ($Logic) { 'src/logic_main.c' } elseif ($ViewTest) { 'tests/view_tests.c' } elseif ($ResourceTest) { 'tests/resource_tests.c' } elseif ($Benchmark) { 'tests/benchmark.c' } elseif ($Test) { 'tests/next_tests.c' } else { 'src/main.c' }
    $output = if ($LogicTest) { 'build/logic-tests' } elseif ($Logic) { 'build/logic' } elseif ($ViewTest) { 'build/view-tests' } elseif ($ResourceTest) { 'build/resource-tests' } elseif ($Benchmark) { 'build/benchmark' } elseif ($Test) { 'build/tests' } else { 'build/zxquest' }
    # Large VM/test control-flow graphs need a bounded register-allocation search.
    $allocationLimit = if ($LogicTest -or $Logic -or $ViewTest) { '--max-allocs-per-node20000' } else { '--max-allocs-per-node200000' }
    $compilerArgs = @('+zxn', '-subtype=nex', '-clib=sdcc_iy', '-startup=0', '-SO3', $allocationLimit, '-pragma-define:CRT_ORG_CODE=32768', '-pragma-define:REGISTER_SP=65280', '-pragma-define:CRT_STACK_SIZE=1024', '-pragma-define:CRT_ENABLE_EIDI=0', '-Isrc', $main, 'src/next/platform.c', 'src/next/resource.c', '-o', $output, '-m', '-create-app')
    if ($LogicTest -or $Logic) { $compilerArgs += @('src/agi/logic.c', 'src/agi/opcodes.c', 'src/next/logic_resource.c','src/next/logic_preload.c') }
    else { $compilerArgs += 'src/agi/picture.c' }
    if ($Test) { $compilerArgs += 'tests/reference_picture.c' }
    if ($ResourceTest -or (!$Test -and !$Benchmark)) { $compilerArgs += @('src/next/resource_cache.c','src/next/resource_cache_load.c','src/next/resource_cache_status.c') }
    if ($ViewTest -or (!$ResourceTest -and !$Test -and !$Benchmark -and !$LogicTest -and !$Logic)) {
        $compilerArgs += @('src/agi/view.c', 'src/next/view_resource.c','src/next/view_setup.c', 'src/next/scene.c')
    }
    & "$Z88dkRoot\bin\zcc.exe" @compilerArgs
    if ($LASTEXITCODE -ne 0) { throw "z88dk build failed ($LASTEXITCODE)" }
    $map = Get-Content -LiteralPath "$output.map" -Raw
    $endMatch = [regex]::Match($map, '(?m)^__BSS_END_tail\s+= \$([0-9A-Fa-f]+)')
    if (!$endMatch.Success) { throw 'Cannot verify resident memory size.' }
    $residentEnd = [Convert]::ToInt32($endMatch.Groups[1].Value, 16)
    if ($residentEnd -gt 0xFB00) { throw 'Resident code/data overlaps the reserved CPU stack.' }
    if ($Benchmark -and $residentEnd -gt 0xF000) { throw 'Benchmark overlaps its IM2 table.' }
    $nexHeader = [IO.File]::ReadAllBytes((Join-Path $PSScriptRoot "$output.nex"))
    if ([Text.Encoding]::ASCII.GetString($nexHeader, 0, 4) -ne 'Next' -or $nexHeader[8] -ne 0) {
        throw 'Expected a NEX for the 1 MB memory target.'
    }
    Write-Host ('Resident code/data ends at ${0:X4}; stack reserved from $FB00.' -f $residentEnd)
    Write-Host "Built $output.nex"
} finally { Pop-Location }
