param([string]$SdRoot=(Split-Path $PSScriptRoot -Parent),[string]$GameDirectory='games/sq1')
$ErrorActionPreference='Stop'
# CSpect's folder-backed F_MKDIR can report success without creating a folder.
# Prepare this on the host; real NextZXOS can also create it in the interpreter.
[uint32[]]$table=@(0,0x1db71064,0x3b6e20c8,0x26d930ac,0x76dc4190,0x6b6b51f4,0x4db26158,0x5005713c,3988292384,4027552580,3604390888,3412177804,2607071920,2262029012,2685067896,3183342108)
[uint32]$crc=4294967295
foreach($name in @('LOGDIR','PICDIR','VIEWDIR','SNDDIR','OBJECT','WORDS.TOK')) {
    $bytes=[IO.File]::ReadAllBytes((Join-Path (Join-Path $SdRoot $GameDirectory) $name))
    foreach($b in ([BitConverter]::GetBytes([uint32]$bytes.Length)+$bytes)) {
        $crc=$crc -bxor [uint32]$b
        $crc=($crc -shr 4) -bxor $table[$crc -band 15]
        $crc=($crc -shr 4) -bxor $table[$crc -band 15]
    }
}
$identity='{0:X8}' -f ($crc -bxor 4294967295)
New-Item -ItemType Directory -Force -Path (Join-Path $SdRoot "saves/$identity") | Out-Null
Write-Host "Save folder: saves/$identity"
