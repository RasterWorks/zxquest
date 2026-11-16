# Host migration tests use synthetic files only, under a new isolated build dir.
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$work=Join-Path $root ('build/save-folder-test-'+[guid]::NewGuid().ToString('N'))
$game=Join-Path $work 'games/demo'
New-Item -ItemType Directory -Path $game -Force | Out-Null
foreach($name in @('LOGDIR','PICDIR','VIEWDIR','SNDDIR','OBJECT','WORDS.TOK')){[IO.File]::WriteAllText((Join-Path $game $name),"fixture $name")}
$prepare=Join-Path $root 'tools/prepare_saves.ps1'
$info=& $prepare -SdRoot $work -GameDirectory 'games/demo' -PassThru
$flat=$info.Directory
$local=Join-Path $flat $info.Identity
$old=Join-Path $work "saves/$($info.Identity)"
New-Item -ItemType Directory -Path $local,$old -Force | Out-Null
function Check([bool]$ok,[string]$name){if(!$ok){throw "FAIL $name"};Write-Host "PASS $name"}
function Put([string]$path,[string]$value){[IO.File]::WriteAllText($path,$value)}
Check ($flat -eq (Join-Path $game 'saves')) 'flat folder is inside game directory'
Put "$old/1.a" 'old a';Put "$old/1.b" 'old b'
Put "$old/2.a" 'wrong location';Put "$local/2.a" 'local a';Put "$local/2.b" 'local b'
Put "$flat/3.a" 'new flat';Put "$old/3.a" 'stale a';Put "$old/3.b" 'stale b'
Put "$local/4.b" 'only b'
$originals=Get-ChildItem -LiteralPath $local,$old -File | Get-FileHash
& $prepare -SdRoot $work -GameDirectory 'games/demo'
Check ((Get-Content -LiteralPath "$flat/1.a") -eq 'old a' -and (Get-Content -LiteralPath "$flat/1.b") -eq 'old b') 'copies a complete launch-root legacy pair'
Check ((Get-Content -LiteralPath "$flat/2.a") -eq 'local a' -and (Get-Content -LiteralPath "$flat/2.b") -eq 'local b') 'prefers game-local legacy pair without mixing locations'
Check ((Get-Content -LiteralPath "$flat/3.a") -eq 'new flat' -and !(Test-Path -LiteralPath "$flat/3.b")) 'existing flat slot is never mixed with a legacy copy'
Check ((Get-Content -LiteralPath "$flat/4.b") -eq 'only b' -and !(Test-Path -LiteralPath "$flat/4.a")) 'imports a legacy slot with only its b file'
Put "$flat/1.a" 'newer save'
& $prepare -SdRoot $work -GameDirectory 'games/demo'
Check ((Get-Content -LiteralPath "$flat/1.a") -eq 'newer save') 'repeated preparation preserves newer flat saves'
$direct=& $prepare -SdRoot $game -GameDirectory '.' -PassThru
Check ($direct.Directory -eq $flat) 'direct and explicit-path launches prepare the same folder'
foreach($entry in $originals){if((Get-FileHash -LiteralPath $entry.Path).Hash -ne $entry.Hash){throw "Legacy original changed: $($entry.Path)"}}
Write-Host 'PASS all legacy originals remain byte-identical'
Write-Host 'SUMMARY checks=8 failures=0'
