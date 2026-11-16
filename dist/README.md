# ZXQuest prebuilt interpreter

[Download ZXQUEST.NEX](https://github.com/RasterWorks/zxquest/raw/refs/heads/main/dist/ZXQUEST.NEX)
and copy it to your Next's SD card. No compiler or emulator is needed.

Put the NEX beside a subfolder for each of your own original DOS/EGA AGI v2
games. Each game folder needs `LOGDIR`, `PICDIR`, `VIEWDIR`, `SNDDIR`, `OBJECT`,
`WORDS.TOK` and every `VOL.*` file. Launch the NEX from NextZXOS; one game loads
directly, or Up/Down and Enter select from several. No CFG is needed for a new
installation. See [full installation and controls](../docs/testing.md).

Target: base 1 MB ZX Spectrum Next, current core/NextZXOS, 28 MHz selected by
the interpreter. SQ1 and KQ1 have been completed on hardware; PQ1 testing is
in progress. Other AGI v2 games are unverified. AGI v3 and SCI are unsupported.
Original game resources and personal saves are not included.

## Build identification

- Build date: 16 November 2026.
- Runtime source: [5cfe6e31bbd044e69c8696c4ca18463de8895bdc](https://github.com/RasterWorks/zxquest/tree/5cfe6e31bbd044e69c8696c4ca18463de8895bdc).
- File size: 131,584 bytes.
- SHA-256: `f53db7c5da85789e5d7fa88b3ca27f0b0461c0b291dae973e06707fe1383fc73`.
- Machine-readable checksum: [SHA256SUMS.txt](SHA256SUMS.txt).

This is the existing tested build, unchanged by its publication here: 150
focused checks and SQ1/KQ1/PQ1 integration passed. Unused slots can still show
`[Unavailable]` on hardware even though save/restore works, and PQ1 driving
can pause. Read the [known issues](../docs/known-issues.md).

The corresponding interpreter source and build scripts are in this repository,
under GPL v2; see [COPYING](../COPYING) and the [build instructions](../README.md#build-and-run-powershell).
Keep the source and licence available when redistributing the binary. Original
Sierra games are separate and are not covered by the interpreter's licence.
