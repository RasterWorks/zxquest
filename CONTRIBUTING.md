# Contributing

Testing on real Next hardware is particularly useful. Start with
[the testing guide](docs/testing.md), then open a bug report with reproducible
steps, your game version and the interpreter build you used. Please report
one distinct problem per issue and check for an existing report first.

Do not commit or attach original Sierra resources, personal save collections,
emulators or toolchains. Generated fixtures and local diagnostics belong under
`build/`; games belong under the ignored `games/` directory.

Code contributions should preserve upstream copyright notices and be provided
under the project's GPL licence. Explain the observable behavior being fixed
and run the relevant checks described in [validation](docs/validation.md).
Tests which use game data require a locally supplied copy.

## Packaging

`tools/package_release.ps1` produces a runtime archive containing only the
interpreter, licence, configuration and installation guide, plus a source
archive. Publish both together from the same tested source revision. Build
with `build.ps1 -Runtime` before packaging any changed runtime sources.

`tools/package_sd.ps1` is for personal SD-card testing: it copies your original
games and saves. Its output must not be uploaded as a public release.
