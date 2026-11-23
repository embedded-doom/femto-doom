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
 *      The statistics of each completed level, written as Chocolate Doom's
 *      -statdump and DOS statdump.exe write them.
 *
 *-----------------------------------------------------------------------------*/

#include <stdio.h>

#include "doomdef.h"
#include "d_player.h"
#include "globdata.h"

#include "statdump.h"

#define MAX_CAPTURES 32

static const int16_t doom1_par_times[] = {30, 75, 120, 90, 165, 180, 180, 30, 165};
static const int16_t doom2_par_times[] = {30, 90, 120, 120, 90, 150, 120, 120, 270};

static wbstartstruct_t captured[MAX_CAPTURES];
static int16_t num_captured;
static gamestate_t previous = GS_DEMOSCREEN;


void Stat_Capture(void)
{
	if (_g_gamestate == GS_INTERMISSION && previous != GS_INTERMISSION && num_captured < MAX_CAPTURES)
		captured[num_captured++] = _g_wminfo;
	previous = _g_gamestate;
}


// 1 for Doom, 2 for Doom II, 0 if the par times cannot tell.
static int16_t Stat_Mission(void)
{
	for (int16_t i = 0; i < num_captured; i++)
	{
		const int16_t level = captured[i].last;
		if (level >= 9)
			return 2;

		const int32_t partime = captured[i].partime;
		const boolean doom1 = partime == doom1_par_times[level] * TIC_RATE;
		const boolean doom2 = partime == doom2_par_times[level] * TIC_RATE;
		if (doom1 && !doom2)
			return 1;
		if (doom2 && !doom1)
			return 2;
	}
	return 0;
}


static void Stat_Percentage(FILE* file, int32_t amount, int32_t total)
{
	if (total == 0)
		fprintf(file, "0");
	else
		fprintf(file, "%li / %li (%i%%)", (long)amount, (long)total, (int16_t)(amount * 100) / (int)total);
}


void Stat_Dump(const char* path)
{
	FILE* file = fopen(path, "wb");
	if (!file)
		return;

	const int16_t mission = Stat_Mission();
	for (int16_t i = 0; i < num_captured; i++)
	{
		const wbstartstruct_t* stats = &captured[i];
		const int16_t level = stats->last + 1;

		fprintf(file, "===========================================\n");
		if (mission == 1)
			fprintf(file, "E1M%i\n", level);
		else if (mission == 2)
			fprintf(file, "MAP%02i\n", level);
		else
			fprintf(file, "E1M%i / MAP%02i\n", level, level);
		fprintf(file, "===========================================\n\n");

		const int16_t time = stats->plyr[0].stime / TIC_RATE;
		const int16_t par = stats->partime / TIC_RATE;
		fprintf(file, "Time: %i:%02i (par: %i:%02i)\n\n", time / 60, time % 60, par / 60, par % 60);

		fprintf(file, "Player 1 (Green):\n\tKills: ");
		Stat_Percentage(file, stats->plyr[0].skills, stats->maxkills);
		fprintf(file, "\n\tItems: ");
		Stat_Percentage(file, stats->plyr[0].sitems, stats->maxitems);
		fprintf(file, "\n\tSecrets: ");
		Stat_Percentage(file, stats->plyr[0].ssecret, stats->maxsecret);
		fprintf(file, "\n\n");
	}
	fclose(file);
}
