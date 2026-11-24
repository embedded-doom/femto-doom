# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2026 Jakob Jungreuthmayer
"""Collects the demos the trace check plays: DEMO1 to DEMO3 of doom1.wad and statcheck's demos
of episode 1 that femto-doom can play.

usage: demos.py DOOM1.WAD STATCHECK OUT
"""

import struct
import sys
from pathlib import Path

DOOM_19 = 109


def playable(demo: bytes) -> bool:
    """Doom 1.9, episode 1, one player, no -respawn, -fast or -nomonsters."""
    if len(demo) < 13:
        return False
    version, _skill, episode, _map, deathmatch, respawn, fast, nomonsters, _console = demo[:9]
    others = demo[10:13]
    return version == DOOM_19 and episode == 1 and not (deathmatch or respawn or fast or nomonsters or any(others))


def wad_demos(path: Path) -> dict[str, bytes]:
    data = path.read_bytes()
    count, directory = struct.unpack_from("<ii", data, 4)
    demos = {}
    for i in range(count):
        offset, size, name = struct.unpack_from("<ii8s", data, directory + 16 * i)
        name = name.rstrip(b"\0").decode()
        if name in ("DEMO1", "DEMO2", "DEMO3"):
            demos[name.lower()] = data[offset : offset + size]
    return demos


def main() -> None:
    iwad, statcheck, out = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    out.mkdir(parents=True, exist_ok=True)
    demos = wad_demos(iwad)
    for lmp in sorted(statcheck.glob("demos/doom/*/e1*/*.lmp")):
        demo = lmp.read_bytes()
        if playable(demo):
            demos[f"{lmp.parent.parent.name}-{lmp.stem}"] = demo
    for name, demo in demos.items():
        (out / f"{name}.lmp").write_bytes(demo)
    print(f"{len(demos)} demos")


if __name__ == "__main__":
    main()
