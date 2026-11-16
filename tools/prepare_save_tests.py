"""Build isolated SD-card tests; never write into supplied game directories/saves."""
from pathlib import Path
import shutil
import zlib
from prepare_logic_tests import ROOT, BUILD, Code, logic

sd = BUILD / 'save-sd'
fixtures = sd / 'build/runtime-fixtures'
for save_dir in [sd / 'saves', fixtures / 'saves', *(sd / 'games' / g / 'saves' for g in ['sq1','kq1','pq1'])]:
    save_dir = save_dir.resolve()
    assert save_dir.is_relative_to(sd.resolve())
    if save_dir.exists():
        for entry in save_dir.rglob('*'):
            assert entry.resolve().is_relative_to(save_dir)
            if entry.is_file():
                entry.unlink()  # isolated diagnostic saves only; retain OneDrive dirs
shutil.copytree(BUILD / 'runtime-fixtures', fixtures, dirs_exist_ok=True)
c = Code().branch('restore', [12, 100]).emit(0x7d, 1, 222)
c.label('restore').branch('restored', [12, 101]).emit(0x7e, 1, 223)
c.label('restored').branch('end', [7, 12]).emit(1, 221).label('end').emit(0)
payload = logic(c.finish())
volume = bytearray((fixtures / 'VOL.0').read_bytes())
index = bytearray((fixtures / 'LOGDIR').read_bytes())
index[0:3] = len(volume).to_bytes(3, 'big')
volume += b'\x12\x34\x00' + len(payload).to_bytes(2, 'little') + payload
(fixtures / 'VOL.0').write_bytes(volume)
(fixtures / 'LOGDIR').write_bytes(index)
crc = 0
for name in ['LOGDIR','PICDIR','VIEWDIR','SNDDIR','OBJECT','WORDS.TOK']:
    data = (fixtures / name).read_bytes()
    crc = zlib.crc32(len(data).to_bytes(4,'little') + data, crc)
(BUILD / 'save_manifest.h').write_text(
    '#define SAVE_ROOT "build/runtime-fixtures/saves"\n'
    f'#define SAVE_LEGACY_LOCAL "build/runtime-fixtures/saves/{crc:08X}"\n'
    f'#define SAVE_LEGACY_ROOT "saves/{crc:08X}"\n', encoding='utf-8')
for game in ['sq1','kq1','pq1']:
    target = sd / 'games' / game
    target.mkdir(parents=True, exist_ok=True)
    for source in (ROOT / 'games' / game).iterdir():
        if source.name.upper() in ('LOGDIR','PICDIR','VIEWDIR','SNDDIR','OBJECT','WORDS.TOK') or source.name.upper().startswith('VOL.'):
            shutil.copyfile(source, target / source.name)
for directory in [fixtures, *(sd / 'games' / g for g in ['sq1','kq1','pq1'])]:
    game_crc = 0
    for name in ['LOGDIR','PICDIR','VIEWDIR','SNDDIR','OBJECT','WORDS.TOK']:
        data = (directory / name).read_bytes()
        game_crc = zlib.crc32(len(data).to_bytes(4,'little') + data, game_crc)
    (directory / 'saves' / f'{game_crc:08X}').mkdir(parents=True, exist_ok=True)
    (sd / 'saves' / f'{game_crc:08X}').mkdir(parents=True, exist_ok=True)
print(f'Isolated SD root: {sd}; fixture save identity {crc:08X}')
# No saves directory at either location: matches a new/flattened installation.
empty = sd / 'build/empty-save-fixtures'
empty.mkdir(parents=True, exist_ok=True)
for source in fixtures.iterdir():
    if source.is_file():
        shutil.copyfile(source, empty / source.name)
# A distinct dictionary fingerprint prevents legacy test slots being found.
with (empty / 'WORDS.TOK').open('ab') as stream:
    stream.write(b'\0')
