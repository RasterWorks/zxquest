"""Copy PQ1 resources/snapshots to an isolated emulator mount; never edit originals."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
sd = ROOT / 'build/driving-sd'
game = sd / 'games/pq1'
saves = game / 'saves'
game.mkdir(parents=True, exist_ok=True)
saves.mkdir(parents=True, exist_ok=True)
(sd / 'build').mkdir(exist_ok=True)
manifest = []


def copy(source, target):
    before = hashlib.sha256(source.read_bytes()).hexdigest()
    shutil.copy2(source, target)
    assert hashlib.sha256(target.read_bytes()).hexdigest() == before
    assert hashlib.sha256(source.read_bytes()).hexdigest() == before
    manifest.append({'source': str(source.relative_to(ROOT)),
                     'copy': str(target.relative_to(sd)), 'sha256': before})


for name in ['LOGDIR', 'PICDIR', 'VIEWDIR', 'SNDDIR', 'OBJECT', 'WORDS.TOK']:
    copy(ROOT / 'games/pq1' / name, game / name)
for path in (ROOT / 'games/pq1').glob('VOL.*'):
    copy(path, game / path.name)
source_saves = ROOT / 'build/sd-card/ZXQUEST/PQ1/saves'
for slot in range(1, 10):
    # Keep a slot pair together; prefer the new flat location when present.
    folder = source_saves if any((source_saves / f'{slot}.{c}').is_file() for c in 'ab') else source_saves / 'C5DDBEA0'
    for path in folder.glob(f'{slot}.[ab]'):
        data = path.read_bytes()
        if data[:8] != b'ZXQSAVE1':
            continue
        length, identity, generation, expected = struct.unpack_from('<IIII', data, 12)
        assert len(data) == 60 + length and identity == 0xC5DDBEA0, path
        assert zlib.crc32(data[:24] + data[28:]) == expected, path
        copy(path, saves / path.name)
        if path.name == '6.a':
            copy(path, saves / '8.a')  # retain the older parked state beside newer 6.b
        print(path.name, data[28:60].split(b'\0')[0].decode('ascii'), 'generation', generation)
(sd / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
(sd / 'ZXQUEST.CFG').write_text('games/pq1\n')
print('Verified isolated mount:', sd)
