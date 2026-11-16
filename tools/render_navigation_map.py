"""Render a ZXQuest packed-picture dump with AGI control/baseline guidance.

Example: python tools/render_navigation_map.py build/kq1-picture-70.raw
Generated maps contain game artwork; keep them out of source distributions.
"""
import argparse
from collections import deque
from pathlib import Path
from PIL import Image, ImageDraw

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--width', type=int, default=8, help='actor baseline width in AGI pixels')
    parser.add_argument('--height', type=int, default=33)
    parser.add_argument('--horizon', type=int, default=36)
    parser.add_argument('--max-y', type=int, default=165, help='last baseline before the lower room-transition strip')
    parser.add_argument('--start', type=int, nargs=2, default=(69,160), metavar=('X','Y'))
    args = parser.parse_args()
    data = args.input.read_bytes()
    if len(data) != 160 * 168 or not 1 <= args.width <= 160:
        raise ValueError('Expected 160x168 packed pixels and a valid actor width')
    palette = [0x000000,0x0000aa,0x00aa00,0x00aaaa,0xaa0000,0xaa00aa,0xaa5500,0xaaaaaa,
               0x555555,0x5555ff,0x55ff55,0x55ffff,0xff5555,0xff55ff,0xffff55,0xffffff]
    rgb = [((c >> 16) & 255, (c >> 8) & 255, c & 255) for c in palette]
    visual = Image.new('RGB', (160,168)); visual.putdata([rgb[p & 15] for p in data])
    priority = Image.new('RGB', (160,168)); priority.putdata([rgb[p >> 4] for p in data])
    safe = visual.copy()
    positions = set()
    for y in range(max(args.height-1,args.horizon+1),min(args.max_y+1,168)):
        for x in range(161-args.width):
            # P2 triggers the script (usually a fall, or dismount at the top).
            # Exclude it here to visualize the continuous climbing corridor.
            if all((data[y*160+i] >> 4) >= 3 for i in range(x,x+args.width)):
                positions.add((x,y))
    start=tuple(args.start)
    if start not in positions:
        raise ValueError(f'Start {start} is not safe for this actor; supply its authored entry position')
    reachable={start};pending=deque([start])
    while pending:
        x,y=pending.popleft()
        for dx,dy in ((0,-1),(1,-1),(1,0),(1,1),(0,1),(-1,1),(-1,0),(-1,-1)):
            point=(x+dx,y+dy)
            if point in positions and point not in reachable:
                reachable.add(point);pending.append(point)
    for point in reachable:safe.putpixel(point,(0,255,255))
    # AGI pixels are twice as wide as display pixels: preserve that aspect.
    scale=(640,336)
    result=Image.new('RGB',(1960,420),'#20252b');draw=ImageDraw.Draw(result)
    for i,(im,title) in enumerate([(visual,'Original picture'),(priority,'Priority / control map'),(safe,'Cyan: reachable safe left-baseline positions')]):
        x=10+i*650;draw.text((x,10),title,fill='white');result.paste(im.resize(scale,Image.Resampling.NEAREST),(x,35))
    draw.text((10,380),f'Actor width: {args.width} AGI pixels. X is the LEFT edge at the feet, not the sprite centre.',fill='white')
    draw.text((10,398),f'Black/blue: blocked. Green (P2): script trigger (fall or upper-room dismount). Start {start}; room scripts also apply.',fill='white')
    output=args.input.with_suffix('.navigation.png');result.save(output);print(output)

if __name__ == '__main__':
    main()
