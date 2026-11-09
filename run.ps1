param([string]$CSpectPath = 'C:\cspect\CSpect.exe', [switch]$Test, [switch]$Benchmark, [switch]$ResourceTest, [switch]$ViewTest, [switch]$LogicTest, [switch]$Logic, [switch]$BankedLogicTest, [switch]$Runtime, [switch]$RuntimeTest, [switch]$ParserTest, [switch]$MotionTest, [switch]$InventoryTest, [switch]$SoundTest, [switch]$SaveTest, [ValidateSet("sq1","kq1","pq1")][string]$Game, [switch]$FreeRun)
$ErrorActionPreference = 'Stop'
if (([int]$Test.IsPresent + [int]$Benchmark.IsPresent + [int]$ResourceTest.IsPresent + [int]$ViewTest.IsPresent + [int]$LogicTest.IsPresent + [int]$Logic.IsPresent + [int]$BankedLogicTest.IsPresent + [int]$Runtime.IsPresent + [int]$RuntimeTest.IsPresent + [int]$ParserTest.IsPresent + [int]$MotionTest.IsPresent + [int]$InventoryTest.IsPresent + [int]$SoundTest.IsPresent + [int]$SaveTest.IsPresent) -gt 1) { throw 'Choose one run mode.' }
$binary = if ($SaveTest) { 'save-tests.nex' } elseif ($SoundTest) { 'sound-tests.nex' } elseif ($InventoryTest) { 'inventory-tests.nex' } elseif ($MotionTest) { 'motion-tests.nex' } elseif ($ParserTest) { 'parser-tests.nex' } elseif ($BankedLogicTest) { 'banked-logic-tests.nex' } elseif ($RuntimeTest) { 'runtime-tests.nex' } elseif ($Runtime) { 'runtime.nex' } elseif ($LogicTest) { 'logic-tests.nex' } elseif ($Logic) { 'logic.nex' } elseif ($ViewTest) { 'view-tests.nex' } elseif ($ResourceTest) { 'resource-tests.nex' } elseif ($Benchmark) { 'benchmark.nex' } elseif ($Test) { 'tests.nex' } else { 'zxquest.nex' }
$nex = Join-Path $PSScriptRoot "build\$binary"
if (!(Test-Path -LiteralPath $nex)) { throw 'Build first using .\build.ps1 (or .\build.ps1 -Test).' }
if ($Game) {
    if (!$Runtime) { throw '-Game requires -Runtime.' }
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot 'ZXQUEST.CFG'),"games/$Game",[Text.Encoding]::ASCII)
}
if ($Runtime) {
    $gameDirectory=if(Test-Path (Join-Path $PSScriptRoot 'ZXQUEST.CFG')){(Get-Content (Join-Path $PSScriptRoot 'ZXQUEST.CFG') -Raw).Trim()}else{'games/sq1'}
    & "$PSScriptRoot/tools/prepare_saves.ps1" -SdRoot $PSScriptRoot -GameDirectory $gameDirectory
}
$sdRoot = $PSScriptRoot.Replace('\', '/') + '/'
if ($SaveTest) { $sdRoot = (Join-Path $PSScriptRoot 'build/save-sd').Replace('\','/') + '/' }
$argsList = @('-w3', '-zxnext', '-s28', '-mouse', '-analytics', ('"-mmc=' + $sdRoot + '"'), ('"' + $nex + '"'))
if ($Runtime) {
    $argsList += @('-basickeys','-esc')
    $CSpectPath = & "$PSScriptRoot/tools/prepare_cspect.ps1" -CSpectPath $CSpectPath
}
$diagnostic = $Test -or $Benchmark -or $ResourceTest -or $ViewTest -or $LogicTest -or $BankedLogicTest -or $RuntimeTest -or $ParserTest -or $MotionTest -or $InventoryTest -or $SoundTest -or $SaveTest
if ($FreeRun -or $diagnostic) { $argsList += '-freerun' }
# CSpect requires audio disabled for uncapped execution.
if ($FreeRun -or $diagnostic) { $argsList += '-sound' }
# CSpect is the interactive program under test; its game window must be visible.
Start-Process -FilePath $CSpectPath -ArgumentList $argsList -WorkingDirectory (Join-Path $PSScriptRoot 'build') -WindowStyle Normal -PassThru
