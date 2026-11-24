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
 *      The screen in memory, in palette indices, and the engine's drawing
 *      into it.
 *
 *-----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <string.h>

#include "screen.h"

#if !defined TEXTURE_SHIFT
#define TEXTURE_SHIFT 0
#endif

#define COLEXTRABITS 7
#define COLBITS      9

#define DOOM_WIDTH   320
#define DOOM_HEIGHT  200


uint8_t screen[SCREEN_HEIGHT][SCREEN_WIDTH];


//
// 3D view, at the top left of the screen
//

static void view_pixel(int x, int y, uint8_t color)
{
	if (0 <= x && x < VIEW_WINDOW_WIDTH && 0 <= y && y < VIEW_WINDOW_HEIGHT)
		screen[y][x] = color;
}


static uint16_t first_frac(const draw_column_vars_t* dcvars)
{
	return (uint16_t)((dcvars->texturemid >> COLEXTRABITS) + (dcvars->yl - CENTER_Y) * (int32_t)dcvars->fracstep);
}


static void draw_column(const draw_column_vars_t* dcvars, int shift)
{
	int count = dcvars->yh - dcvars->yl + 1;
	if (count > SCREEN_HEIGHT)
		count = SCREEN_HEIGHT;

	uint16_t frac = first_frac(dcvars);
	for (int i = 0; i < count; i++)
	{
		view_pixel(dcvars->x, dcvars->yl + i, dcvars->colormap[dcvars->source[frac >> (COLBITS + shift)]]);
		frac += dcvars->fracstep;
	}
}


void R_DrawColumnWall(const draw_column_vars_t* dcvars)
{
	draw_column(dcvars, TEXTURE_SHIFT);
}


void R_DrawColumnSprite(const draw_column_vars_t* dcvars)
{
	draw_column(dcvars, 0);
}


void R_DrawColumnFlat(uint8_t color, const draw_column_vars_t* dcvars)
{
	for (int y = dcvars->yl; y <= dcvars->yh && y < dcvars->yl + SCREEN_HEIGHT; y++)
		view_pixel(dcvars->x, y, color);
}


static const boolean fuzz[50] =
{
	1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1,
	1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1
};

static int fuzzpos;


void R_DrawFuzzColumn(const draw_column_vars_t* dcvars)
{
	int yl = dcvars->yl < 1 ? 1 : dcvars->yl;
	int yh = dcvars->yh > VIEW_WINDOW_HEIGHT - 2 ? VIEW_WINDOW_HEIGHT - 2 : dcvars->yh;
	const uint8_t* shade = fullcolormap + 6 * 256;

	uint16_t frac = first_frac(dcvars) + (yl - dcvars->yl) * dcvars->fracstep;
	for (int y = yl; y <= yh; y++)
	{
		if (fuzz[fuzzpos])
			view_pixel(dcvars->x, y, shade[dcvars->source[frac >> COLBITS]]);

		frac += dcvars->fracstep;
		fuzzpos = (fuzzpos + 1) % 50;
	}
}


void V_ClearViewWindow(void)
{
	for (int y = 0; y < VIEW_WINDOW_HEIGHT; y++)
		memset(screen[y], 0, VIEW_WINDOW_WIDTH);
}


//
// Automap
//

void V_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
	int dx  =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	int dy  = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;

	for (;;)
	{
		view_pixel(x0, y0, color);
		if (x0 == x1 && y0 == y1)
			break;

		int e2 = 2 * err;
		if (e2 >= dy)
		{
			err += dy;
			x0  += sx;
		}
		if (e2 <= dx)
		{
			err += dx;
			y0  += sy;
		}
	}
}


//
// Pictures
//

static void screen_pixel(int x, int y, uint8_t color)
{
	if (0 <= x && x < SCREEN_WIDTH && 0 <= y && y < SCREEN_HEIGHT)
		screen[y][x] = color;
}


static const uint8_t* patch_column(const patch_t* patch, int column)
{
	return (const uint8_t*)patch + (uint16_t)patch->columnofs[column];
}


void V_DrawPatchNotScaled(int16_t x, int16_t y, const patch_t* patch)
{
	x -= patch->leftoffset;
	y -= patch->topoffset;

	for (int column = 0; column < patch->width; column++)
	{
		for (const uint8_t* post = patch_column(patch, column); post[0] != 0xff; post += post[1] + 4)
		{
			for (int i = 0; i < post[1]; i++)
				screen_pixel(x + column, y + post[0] + i, post[3 + i]);
		}
	}
}


#define DX  ((SCREEN_WIDTH << FRACBITS) / DOOM_WIDTH)
#define DXI ((uint16_t)(((DOOM_WIDTH << FRACBITS) / SCREEN_WIDTH) >> 8))
#define DY  (((SCREEN_HEIGHT << FRACBITS) + (1 << FRACBITS) - 1) / DOOM_HEIGHT)
#define DYI ((uint16_t)(((DOOM_HEIGHT << FRACBITS) / SCREEN_HEIGHT) >> 8))


void V_DrawPatchScaled(int16_t x0, int16_t y0, const patch_t* patch)
{
	int x = x0 - patch->leftoffset;
	int y = y0 - patch->topoffset;

	int left   = (x * DX) >> FRACBITS;
	int right  = ((x + patch->width) * DX) >> FRACBITS;
	int bottom = ((y + patch->height) * DY) >> FRACBITS;

	uint16_t col = 0;
	for (int dc_x = left; dc_x < right; dc_x++, col += DXI)
	{
		if (dc_x < 0)
			continue;
		if (dc_x >= SCREEN_WIDTH)
			break;

		for (const uint8_t* post = patch_column(patch, col >> 8); post[0] != 0xff; post += post[1] + 4)
		{
			int dc_yl = ((y + post[0]) * DY) >> FRACBITS;
			if (dc_yl >= SCREEN_HEIGHT || dc_yl > bottom)
				break;

			int dc_yh = ((y + post[0] + post[1]) * DY) >> FRACBITS;
			uint16_t frac = 0;
			for (int dc_y = dc_yl; dc_y < dc_yh && dc_y < dc_yl + SCREEN_HEIGHT; dc_y++, frac += DYI)
				screen_pixel(dc_x, dc_y, post[3 + (frac >> 8)]);
		}
	}
}


void V_DrawBackground(const uint8_t* tile)
{
	for (int y = 0; y < SCREEN_HEIGHT; y++)
		for (int x = 0; x < SCREEN_WIDTH; x++)
			screen[y][x] = tile[(y & 63) * 64 + (x & 63)];
}


void V_DrawRaw(uint32_t offset, const uint8_t* pixels, uint32_t length)
{
	if (offset < sizeof(screen))
		memcpy(&screen[0][0] + offset, pixels, length < sizeof(screen) - offset ? length : sizeof(screen) - offset);
}


void V_DrawRawRect(const uint8_t* pixels, int16_t x, int16_t y, int16_t w, int16_t h)
{
	for (int dy = 0; dy < h; dy++)
		for (int dx = 0; dx < w; dx++)
			screen_pixel(x + dx, y + dy, pixels[dy * SCREEN_WIDTH + dx]);
}
