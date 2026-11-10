# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2026 Jakob Jungreuthmayer
"""Music for one PC speaker voice: Doom's MUS songs reduced to the note started last.

A song becomes pairs of a tone, as in the PC speaker sounds, and its length in 1/140 s. Of the
notes sounding, the one started last plays, of a chord the highest; so the moving part wins over
held ones. A note struck again starts after a tick of silence. The voice moves up by octaves
until its low notes reach the speaker's lowest tone; the few notes still out of range move by
octaves on their own. Percussion is left out.
"""

from __future__ import annotations

import math
import struct
from itertools import pairwise
from typing import NamedTuple

SONGS = (
    "e1m1", "e1m2", "e1m3", "e1m4", "e1m5", "e1m6", "e1m7", "e1m8", "e1m9", "inter", "intro", "victor", "introa",
)  # fmt: skip
PIT = 1_193_182
DIVISORS = (
    0, 6818, 6628, 6449, 6279, 6087, 5906, 5736, 5575, 5423, 5279, 5120, 4971, 4830, 4697, 4554,
    4435, 4307, 4186, 4058, 3950, 3836, 3728, 3615, 3519, 3418, 3323, 3224, 3131, 3043, 2960, 2875,
    2794, 2711, 2633, 2560, 2485, 2415, 2348, 2281, 2213, 2153, 2089, 2032, 1975, 1918, 1864, 1810,
    1757, 1709, 1659, 1612, 1565, 1521, 1478, 1435, 1395, 1355, 1316, 1280, 1242, 1207, 1173, 1140,
    1107, 1075, 1045, 1015, 986, 959, 931, 905, 879, 854, 829, 806, 783, 760, 739, 718, 697, 677,
    658, 640, 621, 604, 586, 570, 553, 538, 522, 507, 493, 479, 465, 452, 439, 427, 415, 403, 391,
    380, 369, 359, 348, 339, 329, 319, 310, 302, 293, 285, 276, 269, 261, 253, 246, 239, 232, 226,
    219, 213, 207, 201, 195, 190, 184, 179,
)  # fmt: skip
"""The PC's timer divisor of each tone, 0 for silence."""
PERCUSSION = 15
LOW_SHARE = 20
"""The song moves up until all but 1 in this many of its notes are in range."""
MAX_TICKS = 255
EVENT_SIZES = {2: 1, 3: 1, 4: 2}


class Event(NamedTuple):
    tick: int
    on: bool
    channel: int
    note: int


def score(mus: bytes) -> tuple[list[Event], int]:
    """The song's note starts and releases, and the tick it ends at."""
    if mus[:4] != b"MUS\x1a" or len(mus) < 8:
        raise ValueError("not a MUS song")
    length, at = struct.unpack_from("<HH", mus, 4)
    body = mus[: at + length]
    tick = 0
    events = []
    try:
        while True:
            byte = body[at]
            at += 1
            kind, channel = byte >> 4 & 7, byte & 15
            if kind == 0:
                events.append(Event(tick, False, channel, body[at] & 127))
                at += 1
            elif kind == 1:
                note = body[at]
                loud = not note & 0x80 or body[at + 1] & 127 != 0
                events.append(Event(tick, loud, channel, note & 127))
                at += 2 if note & 0x80 else 1
            elif kind == 6:
                return events, tick
            else:
                at += EVENT_SIZES.get(kind, 0)
            if byte & 0x80:
                delay = 0
                while True:
                    delay = delay << 7 | body[at] & 127
                    at += 1
                    if not body[at - 1] & 0x80:
                        break
                tick += delay
    except IndexError as error:
        raise ValueError("a MUS song cut short") from error


def frequency(note: int) -> float:
    return 440 * 2 ** ((note - 69) / 12)


def tone(note: int) -> int:
    """The speaker tone nearest to `note`, moved by octaves into range."""
    divisor = PIT / frequency(note)
    while divisor > DIVISORS[1]:
        divisor /= 2
    while divisor < DIVISORS[-1]:
        divisor *= 2
    return min(range(1, len(DIVISORS)), key=lambda t: abs(math.log(DIVISORS[t] / divisor)))


def octaves_up(notes: list[int]) -> int:
    low = sorted(notes)[len(notes) // LOW_SHARE]
    octaves = 0
    while PIT / frequency(low + 12 * octaves) > DIVISORS[1]:
        octaves += 1
    return octaves


def append(pairs: list[list[int]], tone: int, ticks: int) -> None:
    while ticks > 0:
        if pairs and pairs[-1][0] == tone and pairs[-1][1] < MAX_TICKS:
            step = min(ticks, MAX_TICKS - pairs[-1][1])
            pairs[-1][1] += step
        else:
            step = min(ticks, MAX_TICKS)
            pairs.append([tone, step])
        ticks -= step


class Stretch(NamedTuple):
    note: int | None
    ticks: int
    struck: bool


def voice(events: list[Event], end: int) -> list[Stretch]:
    """The note playing from each event to the next, None for none, and whether it was struck."""
    stretches = [Stretch(None, events[0].tick if events else end, False)]
    sounding: dict[tuple[int, int], int] = {}
    for event, following in pairwise([*events, Event(end, False, 0, 0)]):
        if event.on:
            sounding[event.channel, event.note] = event.tick
        else:
            sounding.pop((event.channel, event.note), None)
        last = max(sounding.items(), key=lambda item: (item[1], item[0][1]), default=None)
        note = last[0][1] if last else None
        struck = event.on and last is not None and last[0] == (event.channel, event.note)
        stretches.append(Stretch(note, following.tick - event.tick, struck))
    return stretches


def convert(mus: bytes) -> bytes:
    """The song as a pair count (u16), then per pair a tone (u8) and its ticks (u8)."""
    events, end = score(mus)
    stretches = voice([event for event in events if event.channel != PERCUSSION], end)
    notes = [s.note for s in stretches if s.note is not None and s.ticks > 0]
    if not notes:
        raise ValueError("a song without notes")
    shift = 12 * octaves_up(notes)
    pairs: list[list[int]] = []
    for stretch in stretches:
        current = 0 if stretch.note is None else tone(stretch.note + shift)
        again = bool(pairs) and pairs[-1][0] == current and pairs[-1][1] > 1
        if stretch.struck and stretch.ticks > 0 and again:
            pairs[-1][1] -= 1
            append(pairs, 0, 1)
        append(pairs, current, stretch.ticks)
    return struct.pack("<H", len(pairs)) + b"".join(bytes(pair) for pair in pairs)
