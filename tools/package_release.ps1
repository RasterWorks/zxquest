param([string]$OutputDirectory='build/public-preview')
$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    $output=[IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
    if(Test-Path -LiteralPath $output){throw "Output already exists; choose a fresh directory: $output"}
    $binary=Get-Item -LiteralPath 'build/runtime.nex'
    if([Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($binary.FullName),0,4) -ne 'Next'){throw 'Expected a runtime NEX'}
    $newer=Get-ChildItem src -Recurse -File | Where-Object LastWriteTimeUtc -GT $binary.LastWriteTimeUtc
    if($newer){throw 'Runtime sources changed after the binary. Rebuild with build.ps1 -Runtime first.'}
    $runtime=Join-Path $output 'runtime'
    $source=Join-Path $output 'source'
    New-Item -ItemType Directory -Path $runtime,$source | Out-Null
    Copy-Item -LiteralPath $binary.FullName -Destination (Join-Path $runtime 'ZXQUEST.NEX')
    Copy-Item -LiteralPath 'COPYING' -Destination $runtime
    Copy-Item -LiteralPath 'docs/testing.md' -Destination (Join-Path $runtime 'README.md')
    [IO.File]::WriteAllText((Join-Path $runtime 'ZXQUEST.CFG'),"games/sq1`r`n",[Text.Encoding]::ASCII)
    # Deliberate source allowlist: never recurse over the workspace or SD bundle.
    $files=@('.gitignore','COPYING','README.md','CONTRIBUTING.md','build.ps1','run.ps1')
    foreach($directory in @('src','tests','tools','docs','.github')){
        $files+=Get-ChildItem -LiteralPath $directory -Recurse -File | Where-Object {
            $_.Extension -in @('.c','.h','.asm','.inc','.cs','.py','.ps1','.md','.yml','.yaml','.json') -and
            $_.FullName -notmatch '[\\/]__pycache__[\\/]'
        } | ForEach-Object { $_.FullName.Substring((Get-Location).Path.Length+1) }
    }
    foreach($file in $files){
        $destination=Join-Path $source $file
        New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $file -Destination $destination
    }
    $manifest=@()
    foreach($file in Get-ChildItem -LiteralPath $runtime,$source -Recurse -File){
        $manifest+=('{0}  {1}' -f (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash,$file.FullName.Substring($output.Length+1))
    }
    $manifest | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding ASCII
    Compress-Archive -Path "$runtime/*" -DestinationPath (Join-Path $output 'ZXQuest-runtime.zip')
    Compress-Archive -Path "$source/*" -DestinationPath (Join-Path $output 'ZXQuest-source.zip')
    Write-Output "Prepared runtime and corresponding source in $output. Publish both archives together."
} finally { Pop-Location }
