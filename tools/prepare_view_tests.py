"""Independent host VIEW decoder: test expectations and an inspection sheet.

Generated files stay under build/. Original game files are only read.
"""
from pathlib import Path
import binascii
import json
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
GAME = ROOT / "games/sq1"
PALETTE = [(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),
           (85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]


def word(data, offset):
    assert 0 <= offset <= len(data)-2
    return int.from_bytes(data[offset:offset+2], "little")


def views():
    index = (GAME / "VIEWDIR").read_bytes()
    for number in range(len(index)//3):
        entry = int.from_bytes(index[number*3:number*3+3], "big")
        if entry & 0xFFFFF == 0xFFFFF:
            continue
        volume, offset = entry >> 20, entry & 0xFFFFF
        raw = (GAME / f"VOL.{volume}").read_bytes()
        assert raw[offset:offset+3] == bytes((0x12,0x34,volume))
        size = word(raw,offset+3)
        data = raw[offset+5:offset+5+size]
        assert len(data) == size
        yield number, data


def decode(data, loop, cel):
    loops = data[2]
    assert 0 <= loop < loops and 5+loops*2 <= len(data)
    lo = word(data,5+loop*2)
    count = data[lo]
    assert cel < count and lo >= 5+loops*2
    rel = word(data,lo+1+cel*2)
    assert rel >= 1+count*2
    start = lo+rel
    width, height, flags = data[start:start+3]
    assert width and height
    clear = flags & 15
    mirror = bool(flags & 128) and ((flags >> 4) & 7) != loop
    pos = start+3
    pixels = bytearray()
    for row in range(height):
        scan = bytearray()
        while data[pos]:
            run = data[pos] & 15
            assert run and len(scan)+run <= width, (loop,cel,row,width,data[pos])
            scan.extend([data[pos] >> 4]*run)
            pos += 1
        pos += 1
        scan.extend([clear]*(width-len(scan)))
        pixels.extend(scan[::-1] if mirror else scan)
    return loops,count,width,height,clear,int(mirror),pixels


def main():
    BUILD.mkdir(exist_ok=True)
    records, report, first = bytearray(), [], []
    total = mirrored = pixel_count = 0
    for number,data in views():
        n = 0
        for loop in range(data[2]):
            count = data[word(data,5+loop*2)]
            for cel in range(count):
                loops,count,w,h,clear,mirror,pixels = decode(data,loop,cel)
                records.extend((number,loop,cel,loops,count,w,h,clear,mirror))
                records.extend(binascii.crc_hqx(pixels,0xffff).to_bytes(2,"little"))
                n += 1; total += 1; mirrored += mirror; pixel_count += len(pixels)
                if loop==0 and cel==0:
                    first.append((number,w,h,clear,pixels))
        report.append({"view":number,"loops":data[2],"cels":n})
    (BUILD / "view-expect.bin").write_bytes(records)
    (BUILD / "view_manifest.h").write_text(f"#define EXPECTED_VIEWS {len(report)}\n#define EXPECTED_CELS {total}\n#define EXPECTED_MIRRORED {mirrored}\n")
    (BUILD / "view-inventory.json").write_text(json.dumps(report,indent=2)+"\n")
    fixtures=BUILD / "view-fixtures"
    fixtures.mkdir(exist_ok=True)
    directory=bytearray(b"\xff"*768)
    volume=bytearray()
    for number,loop_at in enumerate((8184,8188,65530)):
        data=bytearray(8220 if number<2 else 65535)
        data[:7]=bytes((1,1,1,0,0,loop_at&255,loop_at>>8))
        data[loop_at:loop_at+3]=bytes((1,3 if number<2 else 20,0))
        if number<2:
            data[loop_at+3:loop_at+12]=bytes((4,2,15,0x12,0x21,0,0xf1,0x32,0))
        directory[number*3:number*3+3]=len(volume).to_bytes(3,"big")
        volume.extend(b"\x12\x34\x00"+len(data).to_bytes(2,"little")+data)
    (fixtures / "VIEWDIR").write_bytes(directory)
    (fixtures / "VOL.0").write_bytes(volume)
    # Contact sheet for identifying existing assets, never used as game input.
    sheet = Image.new("RGB",(768,((len(first)+7)//8)*110),(35,35,35))
    draw = ImageDraw.Draw(sheet)
    for i,(number,w,h,clear,pixels) in enumerate(first):
        cel = Image.new("RGBA",(w,h))
        cel.putdata([(*PALETTE[p],0 if p==clear else 255) for p in pixels])
        scale = min(2,88/(w*2),87/h)
        cel = cel.resize((max(1,round(w*2*scale)),max(1,round(h*scale))),Image.Resampling.NEAREST)
        x,y = (i%8)*96,(i//8)*110
        draw.text((x+3,y+3),str(number),fill="white")
        sheet.paste(cel,(x+(96-cel.width)//2,y+19),cel)
    sheet.save(BUILD / "view-contact-sheet.png")
    print(f"Prepared {len(report)} views, {total} cels ({mirrored} mirrored), {pixel_count} decoded pixels.")


if __name__ == "__main__":
    main()
