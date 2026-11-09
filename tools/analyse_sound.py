"""Read AGI v2 SOUND resources and report playback-planning limits; no writes."""
import argparse
import json
from pathlib import Path


def analyse(directory):
    index = (directory / 'SNDDIR').read_bytes()
    if len(index) % 3:
        raise ValueError('Truncated SOUND directory')
    result = {'resources': [], 'missing_volumes': [], 'zero_duration_events': 0,
              'tone_events': 0, 'noise_events': 0, 'minimum_nonzero_duration': None}
    for number in range(len(index) // 3):
        entry = int.from_bytes(index[3*number:3*number+3], 'big')
        if entry & 0xfffff == 0xfffff:
            continue
        volume, offset = entry >> 20, entry & 0xfffff
        path = directory / f'VOL.{volume}'
        if not path.exists():
            result['missing_volumes'].append({'sound': number, 'volume': volume})
            continue
        raw = path.read_bytes()
        if raw[offset:offset+3] != bytes([0x12, 0x34, volume]) or offset+5 > len(raw):
            raise ValueError(f'SOUND {number}: bad resource header')
        size = int.from_bytes(raw[offset+3:offset+5], 'little')
        data = raw[offset+5:offset+5+size]
        if len(data) != size or size < 8:
            raise ValueError(f'SOUND {number}: truncated payload')
        channels = []
        for ch in range(4):
            at = int.from_bytes(data[2*ch:2*ch+2], 'little')
            count = ticks = 0
            if at < 8:
                raise ValueError(f'SOUND {number}: invalid stream offset')
            while True:
                if at+2 > size:
                    raise ValueError(f'SOUND {number}: missing terminator')
                duration = int.from_bytes(data[at:at+2], 'little')
                if duration == 0xffff:
                    break
                if at+5 > size:
                    raise ValueError(f'SOUND {number}: truncated note')
                if duration:
                    current = result['minimum_nonzero_duration']
                    result['minimum_nonzero_duration'] = duration if current is None else min(current, duration)
                else:
                    result['zero_duration_events'] += 1
                result['noise_events' if ch == 3 else 'tone_events'] += 1
                count += 1
                ticks += duration
                at += 5
            channels.append({'events': count, 'duration_sum': ticks})
        result['resources'].append({'sound': number, 'bytes': size, 'channels': channels})
    result['readable_resources'] = len(result['resources'])
    result['maximum_bytes'] = max((r['bytes'] for r in result['resources']), default=0)
    result['maximum_events'] = max((sum(c['events'] for c in r['channels']) for r in result['resources']), default=0)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', nargs='?', type=Path, default=Path('games/sq1'))
    args = parser.parse_args()
    print(json.dumps(analyse(args.directory), indent=2))
