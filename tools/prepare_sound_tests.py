"""Authored audio resources; original games/sq1 remains read-only."""
from pathlib import Path
from struct import pack

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/sound-fixtures'


def tone(duration, divisor, attenuation=0):
    return pack('<HBBB', duration, divisor >> 4, 0x80 | (divisor & 15), 0x90 | attenuation)


def noise(duration, mode, attenuation=0):
    return pack('<HBBB', duration, 0, 0xe0 | mode, 0xf0 | attenuation)


def sound(channels):
    data = bytearray(8)
    for ch, notes in enumerate(channels):
        data[ch * 2:ch * 2 + 2] = pack('<H', len(data))
        data += b''.join(notes) + b'\xff\xff'
    return data


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    files = {
        1: sound([[tone(2, 100), tone(1, 0), tone(3, 200, 3)],
                  [tone(6, 300, 15)], [tone(6, 400, 6)],
                  [noise(2, 4, 3), noise(2, 0), noise(2, 7, 7)]]),
        2: sound([[], [], [], []]),
        3: bytes(8),
        4: sound([[tone(1, 100)], [], [], []])[:-1],
        5: sound([[tone(0, 100)], [], [], []]),
        7: bytes(6657),
        8: sound([[tone(1, 100)], [tone(1, 200)], [tone(1, 300)], [noise(1, 4)]]),
        9: sound([[tone(65534, 100)], [tone(65534, 200)], [tone(65534, 300)], [noise(65534, 4)]]),
        10: sound([[tone(1, 100 + ch * 100 + i % 17, i % 15) for i in range(300)]
                   for ch in range(3)] + [[noise(1, i % 8, i % 15) for i in range(300)]]),
    }
    files[6] = bytearray(files[1]); files[6][2:4] = files[6][:2]
    index = bytearray(b'\xff' * 768); volume = bytearray()
    for n, data in files.items():
        index[n * 3:n * 3 + 3] = len(volume).to_bytes(3, 'big')
        volume += b'\x12\x34\0' + pack('<H', len(data)) + data
    (OUT / 'SNDDIR').write_bytes(index); (OUT / 'VOL.0').write_bytes(volume)
    for name in ['OBJECT', 'WORDS.TOK']:
        (OUT / name).write_bytes((ROOT / 'build/runtime-fixtures' / name).read_bytes())
    for name in ['LOGDIR', 'PICDIR', 'VIEWDIR']:
        (OUT / name).write_bytes(b'\xff' * 3)
    print('Sound fixtures: 10 resources, maximum', max(map(len, files.values())), 'bytes')


if __name__ == '__main__':
    main()
