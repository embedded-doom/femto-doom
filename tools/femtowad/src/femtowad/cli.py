# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2026 Jakob Jungreuthmayer
"""femto-doom's data tool.

`wad` builds femto-doom's WAD from a Doom IWAD, for the engine's screen size. The maps keep the
IWAD's own nodes. The layout follows jWadUtil by Frenkel Smeijers, which made the WADs of
Doom64KB, the engine femto-doom grew from.

`generate` writes the engine's generated sources: info.c and info.h from info.py, the one
source of thing types, their states and sprites, which `wad` uses too, and r_view.c and
r_view.h, the view tables of every supported view width.

usage: uv run --project tools/femtowad femtowad wad <doom1.wad> <out.wad>
           [--screen WxH] [--texture-shift N] [--high-detail]
           [--wall-column-shift N] [--sprite-column-shift N]
       uv run --project tools/femtowad femtowad generate [--check]
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

from .art import Screen, generate_art
from .graphics import build_graphics, lumps_read_by_name, texture_names
from .info import WAD_VERSION
from .maps import build_maps
from .sourcegen import write_sources
from .wad import Wad


def find_engine() -> Path | None:
    """The femto-doom checkout this tool lives in."""
    return next((d for d in Path(__file__).resolve().parents if (d / "src" / "info.c").is_file()), None)


def wad(args: argparse.Namespace) -> None:
    iwad = Wad.read(args.iwad)
    art = generate_art(iwad, args.screen, lumps_read_by_name())
    graphics = build_graphics(
        iwad, art, args.texture_shift, args.high_detail, args.wall_column_shift, args.sprite_column_shift
    )
    maps = build_maps(iwad, texture_names(iwad))
    graphics["FEMTOWAD"] = struct.pack("<HHH", WAD_VERSION, *args.screen)
    out = Wad([*graphics, *maps])
    data = out.to_bytes()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(data)
    print(f"{args.out}: {len(data)} B, {len(out)} lumps")


def generate(args: argparse.Namespace) -> None:
    if args.engine is None:
        sys.exit("no femto-doom checkout found, pass --engine")
    if not write_sources(args.engine, args.check) and args.check:
        sys.exit(1)


def main() -> None:
    parser = argparse.ArgumentParser(prog="femtowad", description=__doc__.split("\n\n")[0])
    commands = parser.add_subparsers(required=True)

    wad_parser = commands.add_parser("wad", help="build the WAD from an IWAD")
    wad_parser.add_argument("iwad", type=Path, help="Doom IWAD, such as the shareware doom1.wad")
    wad_parser.add_argument("out", type=Path, help="the WAD to write")
    wad_parser.add_argument(
        "--screen", type=Screen.parse, default=Screen(240, 160), help="the engine's SCREEN_WIDTH x SCREEN_HEIGHT"
    )
    wad_parser.add_argument("--texture-shift", type=int, default=0, choices=range(8), help="the engine's TEXTURE_SHIFT")
    wad_parser.add_argument("--high-detail", action="store_true", help="the engine is built with HIGH_DETAIL")
    wad_parser.add_argument(
        "--wall-column-shift",
        type=int,
        default=2,
        choices=range(3),
        help="walls keep every 2^N-th column; fine walls shimmer in the distance",
    )
    wad_parser.add_argument(
        "--sprite-column-shift", type=int, default=0, choices=range(3), help="sprites keep every 2^N-th column"
    )
    wad_parser.set_defaults(run=wad)

    info_parser = commands.add_parser("generate", help="write the engine's generated sources")
    info_parser.add_argument("--check", action="store_true", help="only fail if they differ from info.py")
    info_parser.add_argument("--engine", type=Path, default=find_engine(), help="the femto-doom checkout")
    info_parser.set_defaults(run=generate)

    args = parser.parse_args()
    args.run(args)
