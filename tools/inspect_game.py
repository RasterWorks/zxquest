"""Validate a separate AGI v2 game directory; never modify game data."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def inspect_game(root):
    report = {"directory": str(root), "format": "AGI v2", "indexes": {}, "issues": []}
    volumes = {}
    for name in ("LOGDIR", "PICDIR", "VIEWDIR", "SNDDIR"):
        entries = (root / name).read_bytes()
        if len(entries) % 3 or len(entries) > 768:
            raise ValueError(f"{name}: invalid directory size {len(entries)}")
        resources = []
        for start in range(0, len(entries), 3):
            entry = entries[start:start + 3]
            if int.from_bytes(entry, "big") & 0xfffff == 0xfffff:
                continue
            volume = entry[0] >> 4
            offset = int.from_bytes(entry, "big") & 0xfffff
            if volume not in volumes:
                if not (root / f"VOL.{volume}").is_file():
                    report["issues"].append(f"{name} resource {start // 3}: entry {entry.hex()} references missing VOL.{volume}")
                    continue
                volumes[volume] = (root / f"VOL.{volume}").read_bytes()
            data = volumes[volume]
            header = data[offset:offset + 5]
            label = f"{name} resource {start // 3}"
            if len(header) != 5 or header[:2] != b"\x12\x34" or header[2] != volume:
                report["issues"].append(f"{label}: invalid header at VOL.{volume}:{offset}")
                continue
            size = int.from_bytes(header[3:5], "little")
            if offset + 5 + size > len(data):
                raise ValueError(f"{label}: resource extends beyond volume")
            resources.append({"id": start // 3, "volume": volume, "offset": offset, "bytes": size})
        report["indexes"][name] = {"count": len(resources), "md5": hashlib.md5(entries).hexdigest(),
                                     "maximum_bytes": max((r["bytes"] for r in resources), default=0),
                                     "resources": resources}
    for name in ("OBJECT", "WORDS.TOK"):
        if not (root / name).is_file():
            raise ValueError(f"Missing {name}")
    versions = []
    for name in ("AGI", "AGIDATA.OVL"):
        interpreter = root / name
        if interpreter.is_file():
            versions.extend(s.decode("ascii") for s in re.findall(rb"[ -~]{5,}", interpreter.read_bytes()) if re.search(rb"\b2\.\d{3}\b", s))
    report["interpreter_version_strings"] = list(dict.fromkeys(versions))
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", nargs="?", type=Path, default=Path("games/sq1"))
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    report = inspect_game(args.directory)
    for name, index in report["indexes"].items():
        print(f"{name}: {index['count']} resources; largest {index['maximum_bytes']} bytes")
    for version in report.get("interpreter_version_strings", []):
        print(f"Interpreter: {version}")
    for issue in report["issues"]:
        print(f"WARNING: {issue}")
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    raise SystemExit(1 if report["issues"] else 0)
