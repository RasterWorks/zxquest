"""Small immutable resource-cache fixtures for the display regression suite."""
from pathlib import Path
root = Path(__file__).resolve().parents[1] / 'build' / 'display-fixtures'
root.mkdir(exist_ok=True)
valid = bytes((1,1,2,0,0,9,0,9,0,1,3,0,4,2,0x8f,0x12,0x21,0,0xf1,0x32,0))
bad = bytearray(valid); bad[-2] = 0x35
index = bytearray(b'\xff' * 768); volume = bytearray()
for number, data in enumerate((valid, bad, valid[:-1])):
    index[number*3:number*3+3] = len(volume).to_bytes(3, 'big')
    volume += b'\x12\x34\x00' + len(data).to_bytes(2, 'little') + data
(root/'VIEWDIR').write_bytes(index)
(root/'VOL.0').write_bytes(volume)
print(root)
