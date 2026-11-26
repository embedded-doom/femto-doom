# femto-doom

Doom was released by id Software in 1993 for DOS PCs, requiring at least 4 MB of RAM. Its source
code was published in 1997 and put under the GPL in 1999. Since then Doom has been ported to
almost anything with a screen, most ports loading the game data into RAM and drawing into a frame
buffer.

`femto-doom` is based on [Doom64KB](https://github.com/FrenkelS/Doom64KB) by Frenkel Smeijers,
which runs Doom in 64 KB of RAM with the game data in ROM. `femto-doom` goes further: the shareware
demos run in about 16 KB of RAM, with all game data read in place from read-only memory.

`femto-doom` is a pure Doom engine, without any platform code. The platform hands the game data,
time and input to the engine and draws the engine's output. The whole platform interface is
specified in [i_platform.h](src/i_platform.h).

<p>
<img src="docs/img/title.png" width="32%" alt="The title screen, 240 x 160">
<img src="docs/img/fireball.png" width="32%" alt="Imps and a fireball, 240 x 160">
<img src="docs/img/sergeant.png" width="32%" alt="A sergeant, 240 x 160">
</p>

## Features

- **Game:** shareware episode, all nine maps, all monsters
- **RAM:** packed map data, compact things, sleeping monsters take almost none, no recursion
- **Game data:** read in place, never loaded; optionally through the platform, e.g. from external
  flash
- **Drawing:** no frame buffer; the 3D view column by column and pictures go straight to the
  platform
- **Screen:** any size, 3D view up to 320 pixels wide
- **Build options:** trade detail and speed for memory: wall resolution, lookup tables, per-frame
  limits
- **Code:** plain C11, no operating system needed
- **Automap:** over the 3D view, rotating with the player
- **Floors and light:** floors and ceilings in one colour, light per sector without darkening with
  distance
- **Sound:** PC speaker sound effects and one-voice music, played by the platform
- **Not included:** saving, multiplayer

## Getting started

`femto-doom` ships no game data. [femtowad](tools/femtowad) builds its WAD from Doom's shareware
`doom1.wad`:

```bash
uv tool install ./tools/femtowad
femtowad wad doom1.wad femto.wad
```

A platform compiles [src](src) together with its own code; see [platform.md](docs/platform.md).

- [platform.md](docs/platform.md): building the engine and the platform interface
- [options.h](src/options.h): build options
- [wad-format.md](docs/wad-format.md): the WAD and the tool that builds it

## Example

[host](examples/host) plays `femto-doom` on a desktop, with SDL2.

```bash
make -C examples/host
examples/host/host femto.wad
```

| Action | Key |
|--------|-----|
| move, turn | arrows |
| strafe | `,` `.` |
| fire | Ctrl |
| use | Space, Enter |
| change weapon | Space with `,` or `.` |
| menu | Esc |
| automap | Tab |

## Tests

[run.py](tests/run.py) plays the shareware demos (DEMO1, DEMO2, DEMO3) and the episode 1 demos of
[statcheck](https://github.com/chocolate-doom/statcheck) in a [headless](tests/headless) platform
and in Chocolate Doom (patched with [trace.patch](tests/chocolate/trace.patch)), and compares
their traces tic by tic. Each demo is played in builds with gcc and clang.

```bash
python tests/run.py builtin chocolate-doom doom1.wad
python tests/run.py statcheck chocolate-doom doom1.wad statcheck
```

## History

`femto-doom` descends from a line of ports:

Doom → PrBoom → GBADoom → Doom8088 → Doom64KB → `femto-doom`

| Port | Developer | Target |
|------|-----------|--------|
| [Doom](https://github.com/id-Software/DOOM) | id Software | DOS PCs with 4 MB of RAM |
| [PrBoom](https://prboom.sourceforge.net) | the PrBoom team | modern PCs |
| [GBADoom](https://github.com/doomhack/GBADoom) | doomhack | the Game Boy Advance |
| [Doom8088](https://github.com/FrenkelS/Doom8088) | Frenkel Smeijers | PCs with a 16-bit processor like an 8088 or a 286 |
| [Doom64KB](https://github.com/FrenkelS/Doom64KB) | Frenkel Smeijers | 64 KB of RAM, game data in ROM |
| `femto-doom` | Jakob Jungreuthmayer | about 16 KB of RAM, game data read in place |

## License

GPL-2.0-or-later, see [LICENSE](LICENSE).
