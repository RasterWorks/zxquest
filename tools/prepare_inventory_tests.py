"""Authored inventory/inspection fixtures; original game files stay untouched."""
from prepare_runtime_tests import BUILD, KEY, logic


def object_file(names, carried=()):
    table = bytearray(len(names) * 3)
    text = bytearray()
    for i, name in enumerate(names):
        table[3*i:3*i+3] = (len(table)+len(text)).to_bytes(2, 'little') + bytes([255 if i in carried else 0])
        text += name.encode('ascii') + b'\0'
    return len(table).to_bytes(2, 'little') + b'\0' + table + text


def write_object(folder, raw, encrypted=False):
    folder.mkdir(exist_ok=True)
    if encrypted:
        raw = bytes(c ^ KEY[i % 11] for i, c in enumerate(raw))
    (folder/'OBJECT').write_bytes(raw)


def view(description=b'', rle=bytes([0x21, 0x31, 0x41, 0, 0x51, 0]), width=3, height=2):
    raw = bytearray([1, 1, 1, 0, 0, 7, 0, 1, 3, 0, width, height, 0x9f]) + rle
    if description:
        raw[3:5] = len(raw).to_bytes(2, 'little')
        raw += description + b'\0'
    return raw


def main():
    folder = BUILD/'inventory-fixtures'
    folder.mkdir(exist_ok=True)
    indexes = {name: bytearray(b'\xff'*768) for name in ['LOGDIR', 'PICDIR', 'VIEWDIR', 'SNDDIR']}
    volume = bytearray()

    def add(kind, number, raw):
        indexes[kind][3*number:3*number+3] = len(volume).to_bytes(3, 'big')
        volume.extend(b'\x12\x34\0' + len(raw).to_bytes(2, 'little') + raw)

    add('LOGDIR', 0, logic([0]))
    add('LOGDIR', 1, logic([0x7c, 3, 205, 77, 0]))
    add('LOGDIR', 2, logic([0x81, 1, 3, 205, 88, 0], [b'Nested %s1']))
    add('LOGDIR', 3, logic([0xa2, 206, 3, 205, 99, 0], [b'Nested %s1']))
    add('LOGDIR', 4, logic([0], [b'Name: %n3. Score: %v3.', b'%n255', b'%n256']))
    add('VIEWDIR', 1, view(b'A keycard with %v3 credits. %n3. %m1. Tail.'))
    add('VIEWDIR', 2, view(b'Bad RLE', bytes([0x24, 0, 0])))
    add('VIEWDIR', 3, view(b'Unterminated')[:-1])
    add('VIEWDIR', 4, bytes([1, 1, 1, 255, 255, 7, 0]))
    add('VIEWDIR', 5, view())
    add('VIEWDIR', 6, view(b'A large clipped item.', (bytes([0x2f])*17 + b'\0')*255, 255, 255))
    add('VIEWDIR', 7, view(b'\n'.join([f'Page line {i}'.encode() for i in range(1, 17)])))
    add('VIEWDIR', 8, view(b'X'*512))
    add('VIEWDIR', 9, view(b'W'*511))
    for name, raw in indexes.items():
        (folder/name).write_bytes(raw)
    (folder/'VOL.0').write_bytes(volume)
    (folder/'WORDS.TOK').write_bytes(bytes(52)+b'\0')
    names = ['Pocket Lint', 'Keycard', 'Cartridge', 'Survival Kit', 'A name longer than either inventory column and full row']
    names += [f'Item {i}' for i in range(5, 43)]
    raw = object_file(names, [1, 3, 4, 42])
    write_object(folder, raw, True)
    write_object(BUILD/'inventory-plain', raw)
    write_object(BUILD/'inventory-empty', object_file([]), True)
    write_object(BUILD/'inventory-max', object_file([f'Object {i}' for i in range(256)], range(256)), True)
    write_object(BUILD/'inventory-bad-offset', bytes([3, 0, 0, 2, 0, 255, 0]))
    write_object(BUILD/'inventory-bad-name', bytes([3, 0, 0, 3, 0, 255])+b'not terminated')
    write_object(BUILD/'inventory-short', bytes([6, 0, 0, 6, 0, 255]))
    print('Inventory fixtures generated (encrypted/plain, 0/43/256 objects, 9 views).')


if __name__ == '__main__':
    main()
