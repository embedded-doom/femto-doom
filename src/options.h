/*-----------------------------------------------------------------------------
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *  Copyright 2023-2026 by
 *  Frenkel Smeijers
 *  Copyright 2026 by
 *  Jakob Jungreuthmayer
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *      Build options. Each is set on the compiler's command line, e.g.
 *      -DSCREEN_WIDTH=320 or -DW_FETCH, and all are optional.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __OPTIONS__
#define __OPTIONS__


//==============================================================================================
// Size and detail
//==============================================================================================

// The screen in pixels; must match femtowad wad --screen.
#if !defined SCREEN_WIDTH
#define SCREEN_WIDTH  240
#endif

#if !defined SCREEN_HEIGHT
#define SCREEN_HEIGHT 160
#endif

// The status bar's rows at the bottom of the screen: Doom's 32 of 200 scaled, at least 32.
#define ST_HEIGHT (SCREEN_HEIGHT > 200 ? (32 * SCREEN_HEIGHT + 100) / 200 : 32)

// The 3D view at the top left, by default the screen above the status bar. The width is one of
// 30, 38, 40, 60, 80, 120, 160, 240 and 320, the widths femtowad has view tables for.
#if !defined VIEW_WINDOW_WIDTH
#define VIEW_WINDOW_WIDTH  SCREEN_WIDTH
#endif

#if !defined VIEW_WINDOW_HEIGHT
#define VIEW_WINDOW_HEIGHT (SCREEN_HEIGHT - ST_HEIGHT)
#endif

// Automap width.
#if !defined MAP_WIDTH
#define MAP_WIDTH SCREEN_WIDTH
#endif

// Walls keep every 2^n-th row; must match femtowad wad --texture-shift.
#if !defined TEXTURE_SHIFT
#define TEXTURE_SHIFT 0
#endif

// Colour of nukage floors.
#if !defined NUKAGE_COLOR
#define NUKAGE_COLOR 122
#endif


//==============================================================================================
// Memory
//==============================================================================================

// Wall segments per frame.
#if defined MAX_DRAWSEGS
#elif VIEW_WINDOW_WIDTH * VIEW_WINDOW_HEIGHT <= 38 * 28
#define MAX_DRAWSEGS    64
#else
#define MAX_DRAWSEGS   128
#endif

// Clip range entries per frame.
#if !defined MAX_OPENINGS
#define MAX_OPENINGS (VIEW_WINDOW_WIDTH*16)
#endif

// Lines and things one trace can cross.
#if !defined MAX_INTERCEPTS
#define MAX_INTERCEPTS 128
#endif

// Thing slots kept free at map load.
#if !defined THING_POOL_RESERVE
#define THING_POOL_RESERVE 0
#endif

// A free zone fragment smaller than this stays with the block allocated before it.
#if !defined MIN_FRAGMENT
#define MIN_FRAGMENT 16
#endif

// The division behind reciprocals, e.g. through a divider unit.
#if !defined RECIPROCAL_DIV
#define RECIPROCAL_DIV(v) (0xffffffffu / (v))
#endif


//==============================================================================================
// Switches, off unless defined
//==============================================================================================

// Composed wall textures keep every column instead of every 2^N-th, N from femtowad's
// --wall-column-shift; must match femtowad wad --high-detail.
// #define HIGH_DETAIL

// Walls in one colour.
// #define FLAT_WALL

// A quarter of the sine table, half of the tangent table.
// #define SMALL_TRIG

// WAD reads go through the platform's W_Fetch.
// #define W_FETCH

// No demo in the title loop.
// #define NO_DEMOS

// Maps other than E1M1 and E1M8 spawn only player starts and keys, E1M6 plays E1M1, demos
// play E1M1.
// #define LOW_MEMORY

// The status bar redraws only widgets that changed.
// #define ST_PARTIAL_REFRESH

// No status bar.
// #define DISABLE_STATUS_BAR

// Checks table indices.
// #define RANGE_CHECK

// Logs zone use on every allocation.
// #define INSTRUMENTED

// Checks zone block ids.
// #define ZONEIDCHECK

// Draws every wall with one texture.
// #define ONE_WALL_TEXTURE

#endif
