"""Build independent CRC-16/CCITT expectations and generated edge-case fixtures.

Only writes under build/. Does not convert or modify game files.
"""
from pathlib import Path
import binascii
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
GAME = ROOT / "games/sq1"
NAMES = ("LOGDIR", "PICDIR", "VIEWDIR", "SNDDIR")


def main():
    BUILD.mkdir(exist_ok=True)
    records = []
    hashes = {}
    for kind, name in enumerate(NAMES):
        raw = (GAME / name).read_bytes()
        assert len(raw) % 3 == 0 and len(raw) <= 768, name
        for number in range(len(raw) // 3):
            entry = int.from_bytes(raw[number * 3:number * 3 + 3], "big")
            offset, volume = entry & 0xFFFFF, entry >> 20
            if offset == 0xFFFFF:
                continue
            path = GAME / f"VOL.{volume}"
            if not path.exists():
                # One known anomaly in the supplied SQ1 data, explicitly tested.
                assert (name, number, entry) == ("SNDDIR", 0, 0xFF23C1)
                records.append((kind, number, 2, 0, 0))  # RES_IO
                continue
            contents = path.read_bytes()
            header = contents[offset:offset + 5]
            assert header[:3] == bytes((0x12, 0x34, volume)), (name, number)
            length = int.from_bytes(header[3:], "little")
            payload = contents[offset + 5:offset + 5 + length]
            assert len(payload) == length
            records.append((kind, number, 0, length, binascii.crc_hqx(payload, 0xFFFF)))
    for path in sorted(GAME.iterdir()):
        if path.is_file():
            hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    (BUILD / "resource-source-hashes.json").write_text(json.dumps(hashes, indent=2) + "\n")
    with (BUILD / "resource_manifest.h").open("w") as out:
        out.write("/* Generated from separate game files; no game bytes embedded. */\n")
        out.write("static const struct expected_resource { uint8_t type, number, result; uint16_t length, crc; } expected[] = {\n")
        for kind, number, result, length, crc in records:
            out.write(f"    {{{kind},{number},{result},{length},0x{crc:04x}}},\n")
        out.write("};\n")

    fixtures = BUILD / "resource-fixtures"
    fixtures.mkdir(exist_ok=True)
    directory = bytearray(b"\xff" * 768)
    volume = bytearray()

    def add(number, length, payload=None, signature=b"\x12\x34\x00"):
        directory[number * 3:number * 3 + 3] = len(volume).to_bytes(3, "big")
        volume.extend(signature + length.to_bytes(2, "little"))
        if payload is None:
            payload = bytes((i * 13 + (i >> 8) + number * 17) & 255 for i in range(length))
        volume.extend(payload)

    for number, length in enumerate((0, 8191, 8192, 8193, 65535)):
        add(number, length)
    add(5, 1, b"x", b"\x00\x34\x00")
    add(8, 1, b"x", b"\x12\x00\x00")
    add(9, 1, b"x", b"\x12\x34\x01")
    for number in range(16, 49):
        add(number, 8192)
    # References to EOF and a genuinely short payload must be last.
    directory[10 * 3:10 * 3 + 3] = (len(volume) + 705).to_bytes(3, "big")
    add(6, 9000, bytes(700))
    for name in NAMES:
        (fixtures / name).write_bytes(directory)
    (fixtures / "VOL.0").write_bytes(volume)
    print(f"Prepared {sum(r[2] == 0 for r in records)} resource CRCs, 1 known missing volume, and bank/cache fixtures.")


if __name__ == "__main__":
    main()
