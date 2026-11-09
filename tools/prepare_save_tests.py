"""Build isolated SD-card tests; never write into supplied game directories/saves."""
from pathlib import Path
import json
import shutil
import zlib
from prepare_logic_tests import ROOT, BUILD, Code, logic

sd = BUILD / 'save-sd'
save_dir = (sd / 'saves').resolve()
assert save_dir.is_relative_to((BUILD / 'save-sd').resolve())
if save_dir.exists():
    for entry in save_dir.rglob('*'):
        assert entry.resolve().is_relative_to(save_dir)
        if entry.is_file():
            entry.unlink()  # isolated diagnostic saves only; retain OneDrive dirs
fixtures = sd / 'build/runtime-fixtures'
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
(BUILD / 'save_manifest.h').write_text(f'#define SAVE_ROOT "saves/{crc:08X}"\n', encoding='utf-8')
for game in ['sq1','kq1','pq1']:
    shutil.copytree(ROOT / 'games' / game, sd / 'games' / game, dirs_exist_ok=True)
for directory in [fixtures, *(sd / 'games' / g for g in ['sq1','kq1','pq1'])]:
    game_crc = 0
    for name in ['LOGDIR','PICDIR','VIEWDIR','SNDDIR','OBJECT','WORDS.TOK']:
        data = (directory / name).read_bytes()
        game_crc = zlib.crc32(len(data).to_bytes(4,'little') + data, game_crc)
    (save_dir / f'{game_crc:08X}').mkdir(parents=True, exist_ok=True)
print(f'Isolated SD root: {sd}; fixture save identity {crc:08X}')
