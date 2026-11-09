param([string]$CSpectPath = 'C:\cspect\CSpect.exe')
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
$source=Join-Path $project 'tools/cspect/ZXQuestKeyboard.cs'
$original=(Resolve-Path -LiteralPath $CSpectPath).Path
$sourceDir=Split-Path $original -Parent
$stage=Join-Path $project 'build/cspect'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
# Stage the installed emulator and its existing dependencies, not game assets.
$inputs=@(Get-Item -LiteralPath $original)+@(Get-ChildItem -LiteralPath $sourceDir -File -Filter '*.dll')+@(Get-ChildItem -LiteralPath $sourceDir -File -Filter '*.config')
foreach($inputFile in $inputs) {
    $target=Join-Path $stage $inputFile.Name
    if(!(Test-Path -LiteralPath $target) -or (Get-Item -LiteralPath $target).Length -ne $inputFile.Length -or (Get-Item -LiteralPath $target).LastWriteTimeUtc -ne $inputFile.LastWriteTimeUtc) {
        Copy-Item -LiteralPath $inputFile.FullName -Destination $target
    }
}
$bridge=Join-Path $stage 'ZXQuestKeyboard.dll'
$compiler=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
if(!(Test-Path -LiteralPath $compiler)){$compiler=Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe'}
if(!(Test-Path -LiteralPath $compiler)){throw 'The CSpect keyboard bridge requires the installed .NET Framework C# compiler.'}
if(!(Test-Path -LiteralPath $bridge) -or (Get-Item -LiteralPath $source).LastWriteTimeUtc -gt (Get-Item -LiteralPath $bridge).LastWriteTimeUtc -or (Get-Item (Join-Path $stage 'Plugin.dll')).LastWriteTimeUtc -gt (Get-Item -LiteralPath $bridge).LastWriteTimeUtc) {
    & $compiler /nologo /target:library ("/out:"+$bridge) ("/reference:"+(Join-Path $stage 'Plugin.dll')) $source | Out-Host
    if($LASTEXITCODE -ne 0){throw 'CSpect keyboard bridge compilation failed'}
}
Join-Path $stage (Split-Path $original -Leaf)
