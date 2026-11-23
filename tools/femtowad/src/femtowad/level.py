# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2026 Jakob Jungreuthmayer
"""A map as the engine reads it, its lumps, and the engine's geometry queries on it with the
engine's integer wrapping."""

from __future__ import annotations

import struct
from collections.abc import Iterator, Sequence
from dataclasses import dataclass, field
from typing import NamedTuple

FRACBITS = 16
MAPBLOCKSHIFT = FRACBITS + 7
NF_SUBSECTOR = 0x8000
NO_SIDE = 0xFFFF
NO_SECTOR = 0xFF
ML_TWOSIDED = 4
ML_SOUNDBLOCK = 64

THING = struct.Struct("<hhhBB")
LINE = struct.Struct("<hhhhHhhHHhhhhhBBh")
SIDE = struct.Struct("<hBBBBBB")
SEG = struct.Struct("<hhhhhhHHBB")
SUBSECTOR = struct.Struct("<hh")
NODE = struct.Struct("<hhhh8hHH")
STORED_NODE = struct.Struct("<hhhh8hHH4x")
SECTOR = struct.Struct("<hhhhBBh")


def i32(v: int) -> int:
    return (v + 0x80000000) % 0x100000000 - 0x80000000


def i16(v: int) -> int:
    return (v + 0x8000) % 0x10000 - 0x8000


class Thing(NamedTuple):
    x: int
    y: int
    doomednum: int
    angle: int
    """In steps of 45 degrees."""
    options: int


class Line(NamedTuple):
    v1x: int
    v1y: int
    v2x: int
    v2y: int
    number: int
    dx: int
    dy: int
    front: int
    back: int
    top: int
    bottom: int
    left: int
    right: int
    tag: int
    flags: int
    slope: int
    special: int


class Side(NamedTuple):
    textureoffset: int
    rowoffset: int
    toptexture: int
    bottomtexture: int
    midtexture: int
    sector: int
    changeable: int = 0
    """Numbered from 1 for the sides play can change."""


class Seg(NamedTuple):
    v1x: int
    v1y: int
    v2x: int
    v2y: int
    offset: int
    angle: int
    side: int
    line: int
    frontsector: int
    backsector: int


class Subsector(NamedTuple):
    numsegs: int
    firstseg: int


class Node(NamedTuple):
    x: int
    y: int
    dx: int
    dy: int
    right_box: tuple[int, int, int, int]
    left_box: tuple[int, int, int, int]
    right: int
    left: int

    @classmethod
    def unpack(cls, data: bytes) -> list[Node]:
        return [
            cls(x, y, dx, dy, (b[0], b[1], b[2], b[3]), (b[4], b[5], b[6], b[7]), right, left)
            for x, y, dx, dy, *b, right, left in NODE.iter_unpack(data)
        ]

    def flat(self) -> tuple[int, ...]:
        return (self.x, self.y, self.dx, self.dy, *self.right_box, *self.left_box, self.right, self.left)


class Sector(NamedTuple):
    floorheight: int
    ceilingheight: int
    floorpic: int
    """A palette colour, or FLAT_NUKAGE or FLAT_SKY."""
    ceilingpic: int
    lightlevel: int
    special: int
    tag: int


class Box(NamedTuple):
    top: int
    bottom: int
    left: int
    right: int


def pack(record: struct.Struct, rows: Sequence[tuple[int, ...]]) -> bytes:
    return b"".join(record.pack(*row) for row in rows)


@dataclass
class Level:
    name: str
    things: list[Thing]
    lines: list[Line]
    sides: list[Side]
    segs: list[Seg]
    subsectors: list[Subsector]
    nodes: list[Node]
    sectors: list[Sector]
    reject: bytes
    blockmap: tuple[int, ...]
    """Origin, size, an offset per block, then the line lists the offsets point into."""
    subsector_sector: list[int | None] = field(init=False)

    def __post_init__(self) -> None:
        self.orgx, self.orgy, self.bwidth, self.bheight = self.blockmap[:4]
        self.subsector_sector = [self.first_sector(subsector) for subsector in self.subsectors]

    def lumps(self) -> dict[str, bytes]:
        """In the order the engine expects after the map's marker."""
        return {
            "THINGS": pack(THING, self.things),
            "LINEDEFS": pack(LINE, self.lines),
            "SIDEDEFS": pack(SIDE, self.sides),
            "SEGS": pack(SEG, self.segs),
            "SSECTORS": pack(SUBSECTOR, self.subsectors),
            "NODES": pack(STORED_NODE, [node.flat() for node in self.nodes]),
            "SECTORS": pack(SECTOR, self.sectors),
            "REJECT": self.reject,
            "BLOCKMAP": struct.pack(f"<{len(self.blockmap)}h", *self.blockmap),
        }

    @property
    def numsectors(self) -> int:
        return len(self.sectors)

    @property
    def numblocks(self) -> int:
        return self.bwidth * self.bheight

    def first_sector(self, subsector: Subsector) -> int | None:
        for seg in self.segs[subsector.firstseg : subsector.firstseg + subsector.numsegs]:
            if seg.side != NO_SIDE:
                return self.sides[seg.side].sector
        return None

    def side_sectors(self, line: Line) -> Iterator[int]:
        """The sector in front of the line, then the one behind it if that is another."""
        front = self.sides[line.front].sector
        yield front
        if line.back != NO_SIDE and self.sides[line.back].sector != front:
            yield self.sides[line.back].sector

    @staticmethod
    def point_on_side(x: int, y: int, node: Node) -> int:
        """As R_PointOnSide: 0 for the front, 1 for the back."""
        if not node.dx:
            return int(node.dy > 0) if x <= node.x << FRACBITS else int(node.dy < 0)
        if not node.dy:
            return int(node.dx < 0) if y <= node.y << FRACBITS else int(node.dx > 0)
        x = i32(x - (node.x << FRACBITS))
        y = i32(y - (node.y << FRACBITS))
        if (node.dy ^ node.dx ^ x ^ y) < 0:
            return int((node.dy ^ x) < 0)
        return int(i32((y * node.dx) >> FRACBITS) >= i32((node.dy * x) >> FRACBITS))

    def point_subsector(self, x: int, y: int) -> int:
        if not self.nodes:
            return 0
        n = len(self.nodes) - 1
        while not n & NF_SUBSECTOR:
            node = self.nodes[n]
            n = node.left if self.point_on_side(x, y, node) else node.right
        return n & ~NF_SUBSECTOR

    def point_sector(self, x: int, y: int) -> int | None:
        return self.subsector_sector[self.point_subsector(x, y)]

    def block_coordinate(self, v: int, origin: int) -> int:
        return i16(i32(v - (origin << FRACBITS)) >> MAPBLOCKSHIFT)

    def block(self, x: int, y: int) -> int | None:
        bx, by = self.block_coordinate(x, self.orgx), self.block_coordinate(y, self.orgy)
        if 0 <= bx < self.bwidth and 0 <= by < self.bheight:
            return by * self.bwidth + bx
        return None
