/*-----------------------------------------------------------------------------
 *
 *  Copyright (C) 2026 Jakob Jungreuthmayer
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
 * DESCRIPTION:
 *      A headless platform for the tests: plays a demo file with the screen
 *      in memory and a simulated clock, traces it tic by tic and can save one
 *      frame as a PPM picture.
 *
 *-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i_platform.h"
#include "doomdef.h"
#include "d_player.h"
#include "m_random.h"
#include "globdata.h"

#include "screen.h"


static const uint8_t* palette;
static const uint8_t* pendingpalette;

static uint32_t now_ms;
static uint32_t start_ms;

static uint32_t zone_size = 262144;
static FILE* trace_file;


//
// System
//

uint8_t* I_ZoneBase(uint32_t* heapSize)
{
	uint8_t* zone = calloc(zone_size + 16, 1);
	if (!zone)
		I_Error("I_ZoneBase: no memory for a %lu byte zone", (unsigned long)zone_size);

	*heapSize = zone_size;
	return zone + (16 - (uintptr_t)zone % 16) % 16;
}


static uint32_t clock_ms(void)
{
	return ++now_ms;
}


int32_t I_GetTime(void)
{
	return (int32_t)((uint64_t)(clock_ms() - start_ms) * TIC_RATE / 1000);
}


void I_StartTic(void)
{
}


void I_Quit(void)
{
	if (trace_file)
		fclose(trace_file);
	exit(0);
}


void I_Log(const char* text)
{
	fputs(text, stdout);
}


void I_Fatal(const char* message)
{
	fputs(message, stderr);
	fputc('\n', stderr);

	exit(1);
}


#if defined W_FETCH
void W_Fetch(const void* src, void* dst, uint16_t len)
{
	memcpy(dst, src, len);
}
#endif


//
// Sound
//

void DMX_PlaySound(const uint8_t* sound, uint8_t volume)
{
	(void)sound;
	(void)volume;
}


void DMX_PlaySong(const uint8_t* song, boolean looping)
{
	(void)song;
	(void)looping;
}


//
// Frame
//

void I_SetPalette(const uint8_t* pal)
{
	if (palette)
		pendingpalette = pal;
	else
		palette = pal;
}


void I_FinishUpdate(void)
{
	if (pendingpalette)
	{
		palette = pendingpalette;
		pendingpalette = NULL;
	}
}


void I_StartWipe(void)
{
}


void I_FinishWipe(void)
{
	I_FinishUpdate();
}


//
// The demo
//

static void save_ppm(const char* path, const uint8_t* pal)
{
	FILE* file = fopen(path, "wb");
	if (!file)
		I_Error("cannot write %s", path);

	fprintf(file, "P6 %d %d 255\n", SCREEN_WIDTH, SCREEN_HEIGHT);
	for (size_t i = 0; i < sizeof(screen); i++)
		fwrite(&pal[(&screen[0][0])[i] * 3], 1, 3, file);
	fclose(file);
}


static const uint8_t* load(const char* path, long* length)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		I_Error("cannot read %s", path);

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	uint8_t* data = malloc(size);
	if (!data || fread(data, 1, size, file) != (size_t)size)
		I_Error("cannot read %s", path);

	fclose(file);
	if (length)
		*length = size;
	return data;
}


static uint32_t word(const uint8_t* p)
{
	return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}


static void set_word(uint8_t* p, uint32_t v)
{
	p[0] = v;
	p[1] = v >> 8;
	p[2] = v >> 16;
	p[3] = v >> 24;
}


// The graphics WAD with its DEMO3 lump replaced by the demo in path.
static const uint8_t* with_demo(const char* wad_path, const char* path)
{
	long wad_size, demo_size;
	const uint8_t* wad = load(wad_path, &wad_size);
	const uint8_t* demo = load(path, &demo_size);
	const long at = (wad_size + 31) & ~31L;
	uint8_t* copy = calloc(at + demo_size, 1);
	if (!copy)
		I_Error("no memory for %s", path);
	memcpy(copy, wad, wad_size);
	memcpy(copy + at, demo, demo_size);

	for (uint32_t i = 0; i < word(copy + 4); i++)
	{
		uint8_t* entry = copy + word(copy + 8) + 16 * i;
		if (!strncmp((const char*)entry + 8, "DEMO3", 8))
		{
			set_word(entry, at);
			set_word(entry + 4, demo_size);
			return copy;
		}
	}
	I_Error("%s has no DEMO3", wad_path);
	return NULL;
}


// Per tic of a demo's levels: level time, player position, angle and health, its kills, items
// and secrets, then where P_Random is.
static void trace_tic(void)
{
	static int32_t last = -1;
	if (!trace_file || !_g_demoplayback || _g_gamestate != GS_LEVEL || !_g_player.mo || _g_gametic == last)
		return;
	last = _g_gametic;
	const mobj_t* mo = _g_player.mo;
	fprintf(trace_file, "T %ld %ld %ld %lu %d %d %d %d\nR %u\n", (long)_g_leveltime, (long)mo->x, (long)mo->y,
		(unsigned long)mo->angle, _g_player.health, _g_player.killcount, _g_player.itemcount,
		_g_player.secretcount, P_RandomIndex());
}


static void usage(void)
{
	fprintf(stderr,
		"usage: headless GRAPHICS.WAD MAPS.WAD DEMO.lmp [-nodraw] [-trace FILE]\n"
		"                [-shot FRAME FILE.ppm] [-zone BYTES]\n");
	exit(2);
}


int main(int argc, char** argv)
{
	long shot = -1;
	const char* shot_file = NULL;
	boolean nodraw = false;

	if (argc < 4)
		usage();

	for (int i = 4; i < argc; i++)
	{
		if (!strcmp(argv[i], "-zone") && i + 1 < argc)
			zone_size = strtoul(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "-nodraw"))
			nodraw = true;
		else if (!strcmp(argv[i], "-trace") && i + 1 < argc)
		{
			trace_file = fopen(argv[++i], "wb");
			if (!trace_file)
				I_Error("cannot write %s", argv[i]);
		}
		else if (!strcmp(argv[i], "-shot") && i + 2 < argc)
		{
			shot = atol(argv[++i]);
			shot_file = argv[++i];
		}
		else
			usage();
	}

	const char* engine_argv[4] = {"femto-doom", "-timedemo", "demo3"};
	int engine_argc = 3;
	if (nodraw)
		engine_argv[engine_argc++] = "-nodraw";

	start_ms = clock_ms();
	W_SetWads(with_demo(argv[1], argv[3]), load(argv[2], NULL));
	D_DoomInit(engine_argc, engine_argv);

	for (long f = 0;; f++)
	{
		const uint8_t* shown = palette;
		D_DoomStep();
		trace_tic();

		if (f == shot && !nodraw)
			save_ppm(shot_file, shown);
	}
}
