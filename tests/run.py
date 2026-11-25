# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2026 Jakob Jungreuthmayer
"""Plays demos in femto-doom and in Chocolate Doom built with chocolate/trace.patch, and
compares their traces tic by tic.

builtin plays the demos built into doom1.wad, DEMO1 to DEMO3, drawn. statcheck plays the
episode 1 demos of statcheck that femto-doom can play, without drawing. Each demo is played in
every build of the headless platform; CFLAGS adds compiler flags.
"""

import argparse
import os
import shlex
import struct
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "tests" / "out"
EXE = ".exe" if os.name == "nt" else ""

BUILDS = {
    "gcc": "gcc",
    "clang": "clang",
    "clang-m32": "clang -m32",
}

DOOM_19 = 109


def playable(demo: bytes) -> bool:
    """Doom 1.9, episode 1, one player, no -respawn, -fast or -nomonsters."""
    if len(demo) < 13:
        return False
    version, _skill, episode, _map, deathmatch, respawn, fast, nomonsters, _console = demo[:9]
    others = demo[10:13]
    return version == DOOM_19 and episode == 1 and not (deathmatch or respawn or fast or nomonsters or any(others))


def wad_demos(iwad: Path) -> dict[str, bytes]:
    data = iwad.read_bytes()
    count, directory = struct.unpack_from("<ii", data, 4)
    demos = {}
    for i in range(count):
        offset, size, name = struct.unpack_from("<ii8s", data, directory + 16 * i)
        name = name.rstrip(b"\0").decode()
        if name in ("DEMO1", "DEMO2", "DEMO3"):
            demos[name.lower()] = data[offset : offset + size]
    return demos


def statcheck_demos(statcheck: Path) -> dict[str, bytes]:
    demos = {}
    for lmp in sorted(statcheck.glob("demos/doom/*/e1*/*.lmp")):
        demo = lmp.read_bytes()
        if playable(demo):
            demos[f"{lmp.parent.parent.name}-{lmp.stem}"] = demo
    return demos


def chocolate(doom: Path, iwad: Path, lmp: Path, trace: Path) -> None:
    env = os.environ | {"SDL_VIDEODRIVER": "dummy", "SDL_AUDIODRIVER": "dummy", "CHOCO_TRACE": str(trace)}
    args: list[str | Path] = [doom, "-iwad", iwad, "-timedemo", lmp, "-nodraw", "-nosound", "-nomusic", "-nogui"]
    with tempfile.TemporaryDirectory() as config:
        args += ["-config", Path(config) / "default.cfg", "-extraconfig", Path(config) / "extra.cfg"]
        subprocess.run(args, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    if not trace.exists() or trace.stat().st_size == 0:
        sys.exit(f"Chocolate Doom left no trace for {lmp.name}")


def build(name: str, cc: str, out: Path) -> Path:
    out.mkdir(parents=True, exist_ok=True)
    exe = out / f"headless{EXE}"
    args = [
        *shlex.split(cc),
        "-std=c11",
        "-fno-short-enums",
        "-O2",
        "-Wall",
        "-Wextra",
        "-Werror",
        *shlex.split(os.environ.get("CFLAGS", "")),
        f"-I{ROOT / 'src'}",
        f"-I{ROOT / 'examples' / 'host'}",
        "-o",
        str(exe),
        str(ROOT / "tests" / "headless" / "headless.c"),
        str(ROOT / "examples" / "host" / "screen.c"),
        *map(str, sorted((ROOT / "src").glob("*.c"))),
    ]
    if subprocess.run(args, check=False).returncode:
        sys.exit(f"{name}: build failed")
    return exe


def first_difference(got: list[str], want: list[str]) -> int | None:
    for i, (a, b) in enumerate(zip(got, want, strict=False)):
        if a != b:
            return i
    return None if len(got) == len(want) else min(len(got), len(want))


def femto(exe: Path, wad: Path, lmp: Path, want: Path, drawn: bool) -> str | None:
    """Plays the demo and returns how its trace differs from Chocolate Doom's, if it does."""
    trace = exe.parent / f"{lmp.stem}.trace"
    args: list[str | Path] = [exe, wad, lmp, "-trace", trace]
    run = subprocess.run([*args, *([] if drawn else ["-nodraw"])], capture_output=True, text=True, check=False)
    got = trace.read_text().splitlines() if trace.exists() else []
    trace.unlink(missing_ok=True)
    expected = want.read_text().splitlines()
    i = first_difference(got, expected)
    if i is None:
        return None
    lines = slice(max(i - 1, 0), i + 1)
    return "\n".join(
        [
            f"{lmp.stem}, line {i + 1}:",
            f"  femto-doom:     {' '.join(got[lines])}",
            f"  Chocolate Doom: {' '.join(expected[lines])}",
            *(f"  {line}" for line in run.stderr.splitlines()),
            *([f"  exit code {run.returncode}"] if run.returncode else []),
        ]
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("demos", choices=["builtin", "statcheck"])
    parser.add_argument("chocolate", type=Path, help="chocolate-doom built with chocolate/trace.patch")
    parser.add_argument("iwad", type=Path, help="doom1.wad")
    parser.add_argument("statcheck", type=Path, nargs="?", help="statcheck checkout, for statcheck")
    args = parser.parse_args()
    if (args.demos == "statcheck") != (args.statcheck is not None):
        parser.error("statcheck needs the statcheck checkout, builtin none")

    out = OUT / args.demos
    demos = wad_demos(args.iwad) if args.demos == "builtin" else statcheck_demos(args.statcheck)
    (out / "demos").mkdir(parents=True, exist_ok=True)
    (out / "chocolate").mkdir(exist_ok=True)
    lmps = []
    for name, demo in demos.items():
        lmp = out / "demos" / f"{name}.lmp"
        lmp.write_bytes(demo)
        lmps.append(lmp)
    traces = [out / "chocolate" / f"{lmp.stem}.trace" for lmp in lmps]
    print(f"{len(lmps)} demos", flush=True)

    with ThreadPoolExecutor(os.cpu_count()) as pool:
        list(pool.map(partial(chocolate, args.chocolate, args.iwad), lmps, traces))
    print("Chocolate Doom traced them", flush=True)

    wad = OUT / "femto.wad"
    femtowad = ["uv", "run", "--project", str(ROOT / "tools" / "femtowad"), "femtowad"]
    subprocess.run([*femtowad, "wad", str(args.iwad), str(wad)], stdout=subprocess.DEVNULL, check=True)

    failed = False
    for name, cc in BUILDS.items():
        exe = build(name, cc, out / name)
        with ThreadPoolExecutor(os.cpu_count()) as pool:
            diffs = [d for d in pool.map(partial(femto, exe, wad, drawn=args.demos == "builtin"), lmps, traces) if d]
        print(f"{name}: {len(lmps) - len(diffs)} of {len(lmps)} match", flush=True)
        for d in diffs:
            print(d)
        failed |= bool(diffs)
    sys.exit(failed)


if __name__ == "__main__":
    main()
