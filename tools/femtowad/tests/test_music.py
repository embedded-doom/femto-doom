# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2026 Jakob Jungreuthmayer
from __future__ import annotations

import struct

import pytest

from femtowad.graphics import SOUNDS, add_audio
from femtowad.music import DIVISORS, SONGS, convert, score, tone
from femtowad.wad import Lump, Wad


def mus(*events: int) -> bytes:
    body = bytes(events) + bytes([0x60])
    return b"MUS\x1a" + struct.pack("<HHHHHH", len(body), 16, 1, 0, 0, 0) + body


def pairs(song: bytes) -> list[tuple[int, int]]:
    count = struct.unpack_from("<H", song)[0]
    return [(song[2 + 2 * i], song[3 + 2 * i]) for i in range(count)]


def test_tone() -> None:
    assert DIVISORS[tone(69)] == 2711
    assert tone(33) == tone(45) == tone(57)
    assert DIVISORS[tone(57)] == 5423


def test_score() -> None:
    events, end = score(mus(0x10, 69, 0x91, 0x80 | 72, 100, 10, 0x00, 69))
    assert events == [(0, True, 0, 69), (0, True, 1, 72), (10, False, 0, 69)]
    assert end == 10


def test_highest_of_a_chord() -> None:
    song = convert(mus(0x10, 69, 0x90, 72, 5, 0x80, 72, 7, 0x80, 69, 3))
    assert pairs(song) == [(tone(72), 5), (tone(69), 7), (0, 3)]


def test_last_started_note() -> None:
    song = convert(mus(0x90, 72, 5, 0x91, 69, 4, 0x81, 69, 2, 0x80, 72, 1))
    assert pairs(song) == [(tone(72), 5), (tone(69), 4), (tone(72), 2), (0, 1)]


def test_struck_again() -> None:
    song = convert(mus(0x90, 69, 5, 0x00, 69, 0x90, 69, 5, 0x80, 69, 0))
    assert pairs(song) == [(tone(69), 4), (0, 1), (tone(69), 5)]


def test_rest_first() -> None:
    song = convert(mus(0xB0, 10, 3, 0x90, 69, 5, 0x00, 69))
    assert pairs(song) == [(0, 3), (tone(69), 5)]


def test_zero_volume_releases() -> None:
    events, _ = score(mus(0x90, 0x80 | 69, 0, 5))
    assert events == [(0, False, 0, 69)]


def test_low_song_moves_up_whole() -> None:
    song = convert(mus(0x90, 33, 5, 0x00, 33, 0x90, 45, 5, 0x00, 45))
    (low, _), (high, _) = pairs(song)
    assert DIVISORS[low] / DIVISORS[high] == pytest.approx(2, rel=0.02)


def test_percussion_only() -> None:
    with pytest.raises(ValueError, match="without notes"):
        convert(mus(0x9F, 36, 5, 0x0F, 36))


def test_cut_short() -> None:
    with pytest.raises(ValueError, match="cut short"):
        score(mus(0x90, 69)[:-2])


def test_audio_consecutive_in_engine_order() -> None:
    song = mus(0x90, 69, 5, 0x00, 69)
    sounds = [f"DP{sound.upper()}" for sound in SOUNDS]
    songs = [f"D_{song.upper()}" for song in SONGS]
    iwad = Wad(Lump(name, b"\0\0\1\0\1") for name in [*reversed(sounds), "DPBDOPN"])
    iwad.lumps += [Lump(name, song) for name in songs]
    wad = Wad([Lump("PLAYPAL", b"")])
    add_audio(wad, iwad)
    assert [lump.name for lump in wad] == ["PLAYPAL", *sounds, *songs]


def test_long_notes_split() -> None:
    song = convert(mus(0x90, 69, 0x82, 0x2C, 0x00, 69))
    assert pairs(song) == [(tone(69), 255), (tone(69), 45)]
