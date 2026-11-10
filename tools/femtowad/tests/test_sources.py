# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2026 Jakob Jungreuthmayer
from __future__ import annotations

import re

from femtowad.cli import find_engine
from femtowad.graphics import SOUNDS
from femtowad.music import SONGS
from femtowad.sourcegen import write_sources


def test_generated_sources_are_current() -> None:
    engine = find_engine()
    assert engine is not None
    assert write_sources(engine, check=True)


def test_sounds_in_the_engine_order() -> None:
    engine = find_engine()
    assert engine is not None
    names = re.findall(r"^\s*sfx_(\w+),", (engine / "src" / "sounds.h").read_text(), re.MULTILINE)
    assert tuple(names) == ("None", *SOUNDS)


def test_songs_in_the_engine_order() -> None:
    engine = find_engine()
    assert engine is not None
    names = re.findall(r"^\s*mus_(\w+),", (engine / "src" / "sounds.h").read_text(), re.MULTILINE)
    assert tuple(names) == ("None", *SONGS)
