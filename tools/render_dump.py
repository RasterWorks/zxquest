"""Render packed picture or Layer 2 test dumps (requires Pillow)."""
import argparse
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("input", type=Path, nargs="?", default=Path("build/pic001.raw"))
parser.add_argument("output", type=Path, nargs="?", default=Path("build/pic001.png"))
parser.add_argument("--layer2", action="store_true", help="Read an 80 KB 320x256 column-major framebuffer")
args = parser.parse_args()
pixels = args.input.read_bytes()
expected = 320 * 256 if args.layer2 else 160 * 168
if len(pixels) != expected:
    raise ValueError(f"Expected exactly {expected} pixels")
palette = [0x000000, 0x0000aa, 0x00aa00, 0x00aaaa, 0xaa0000, 0xaa00aa,
           0xaa5500, 0xaaaaaa, 0x555555, 0x5555ff, 0x55ff55, 0x55ffff,
           0xff5555, 0xff55ff, 0xffff55, 0xffffff]
rgb_palette = [(c >> 16, (c >> 8) & 255, c & 255) for c in palette]
height = 256 if args.layer2 else 168
image = Image.new("RGB", (320, height))
if args.layer2:
    image.putdata([rgb_palette[pixels[x * 256 + y] & 15] for y in range(256) for x in range(320)])
else:
    image.putdata([rgb_palette[p & 15] for p in pixels for _ in range(2)])
image.resize((640, height * 2), Image.Resampling.NEAREST).save(args.output)
print(args.output)
