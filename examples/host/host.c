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
 *      A desktop platform on SDL2: the screen in a window, the keyboard, and
 *      the PC speaker's sounds and music as square waves.
 *
 *-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "i_platform.h"
#include "screen.h"

#define ZONE_SIZE   131072

#define RATE        44100
#define TONE_SAMPLES (RATE / 140)
#define AMPLITUDE   3000
#define TIMER_FREQ  1193181


static SDL_Window* window;
static SDL_Renderer* renderer;
static SDL_Texture* texture;
static SDL_AudioDeviceID audio;

static const uint8_t* palette;
static const uint8_t* pendingpalette;

static uint32_t start_ms;


//
// System
//

uint8_t* I_ZoneBase(uint32_t* heapSize)
{
	uint8_t* zone = malloc(ZONE_SIZE);
	if (!zone)
		I_Error("I_ZoneBase: no memory for the zone");

	*heapSize = ZONE_SIZE;
	return zone;
}


int32_t I_GetTime(void)
{
	static int32_t last;
	int32_t now = (int32_t)((uint64_t)(SDL_GetTicks() - start_ms) * TIC_RATE / 1000);
	if (now == last)
		SDL_Delay(1);
	last = now;
	return now;
}


static int16_t key(SDL_Keycode sym)
{
	switch (sym)
	{
		case SDLK_UP:     return KEYD_UP;
		case SDLK_DOWN:   return KEYD_DOWN;
		case SDLK_LEFT:   return KEYD_LEFT;
		case SDLK_RIGHT:  return KEYD_RIGHT;
		case SDLK_LCTRL:
		case SDLK_RCTRL:  return KEYD_B;
		case SDLK_SPACE:
		case SDLK_RETURN: return KEYD_A;
		case SDLK_COMMA:  return KEYD_L;
		case SDLK_PERIOD: return KEYD_R;
		case SDLK_ESCAPE: return KEYD_START;
		case SDLK_TAB:    return KEYD_SELECT;
		default:          return -1;
	}
}


void I_StartTic(void)
{
	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		if (e.type == SDL_QUIT)
			I_Quit();

		if ((e.type == SDL_KEYDOWN && !e.key.repeat) || e.type == SDL_KEYUP)
		{
			event_t ev = {0};
			ev.type  = e.type == SDL_KEYDOWN ? ev_keydown : ev_keyup;
			ev.data1 = key(e.key.keysym.sym);
			if (ev.data1 >= 0)
				D_PostEvent(&ev);
		}
	}
}


void I_Quit(void)
{
	SDL_Quit();
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

	SDL_Quit();
	exit(1);
}


#if defined W_FETCH
void W_Fetch(const void* src, void* dst, uint16_t len)
{
	memcpy(dst, src, len);
}
#endif


//
// Sound: one PC speaker voice, the sound effect over the song
//

static const uint16_t divisors[128] =
{
	   0,
	6818, 6628, 6449, 6279, 6087, 5906, 5736, 5575,
	5423, 5279, 5120, 4971, 4830, 4697, 4554, 4435,
	4307, 4186, 4058, 3950, 3836, 3728, 3615, 3519,
	3418, 3323, 3224, 3131, 3043, 2960, 2875, 2794,
	2711, 2633, 2560, 2485, 2415, 2348, 2281, 2213,
	2153, 2089, 2032, 1975, 1918, 1864, 1810, 1757,
	1709, 1659, 1612, 1565, 1521, 1478, 1435, 1395,
	1355, 1316, 1280, 1242, 1207, 1173, 1140, 1107,
	1075, 1045, 1015,  986,  959,  931,  905,  879,
	 854,  829,  806,  783,  760,  739,  718,  697,
	 677,  658,  640,  621,  604,  586,  570,  553,
	 538,  522,  507,  493,  479,  465,  452,  439,
	 427,  415,  403,  391,  380,  369,  359,  348,
	 339,  329,  319,  310,  302,  293,  285,  276,
	 269,  261,  253,  246,  239,  232,  226,  219,
	 213,  207,  201,  195,  190,  184,  179
};

static struct
{
	const uint8_t* tones;
	uint16_t count;
	uint16_t at;
	uint32_t samples;
	int16_t amplitude;
} sound;

static struct
{
	const uint8_t* pairs;
	uint16_t count;
	uint16_t at;
	boolean looping;
	uint8_t tone;
	uint32_t samples;
	uint32_t drum;
} song;

static uint32_t phase;
static uint32_t noise = 1;


static uint16_t word(const uint8_t* p)
{
	return p[0] | p[1] << 8;
}


static uint32_t frequency(uint8_t tone)
{
	return tone < 128 && divisors[tone] ? TIMER_FREQ / divisors[tone] : 0;
}


static void next_pair(void)
{
	for (uint32_t n = 0; !song.samples && song.pairs && n <= song.count; n++)
	{
		if (song.at == song.count)
		{
			if (!song.looping)
			{
				song.pairs = NULL;
				song.tone = 0;
				return;
			}
			song.at = 0;
		}

		const uint8_t* pair = song.pairs + 2 + 2 * song.at++;
		if (pair[0] >= 128)
			song.drum = RATE / 50;
		else
		{
			song.tone = pair[0];
			song.samples = pair[1] * TONE_SAMPLES;
		}
	}
}


static void mix(void* userdata, Uint8* stream, int len)
{
	(void)userdata;
	int16_t* out = (int16_t*)stream;

	for (int i = 0; i < len / 2; i++)
	{
		uint32_t freq = 0;
		int16_t amplitude = AMPLITUDE / 2;

		next_pair();
		if (song.pairs && song.samples)
		{
			freq = frequency(song.tone);
			song.samples--;
		}

		if (sound.tones)
		{
			freq = frequency(sound.tones[sound.at]);
			amplitude = sound.amplitude;
			if (++sound.samples == TONE_SAMPLES)
			{
				sound.samples = 0;
				if (++sound.at == sound.count)
					sound.tones = NULL;
			}
		}

		phase = (phase + freq) % RATE;
		out[i] = freq ? (phase < RATE / 2 ? amplitude : -amplitude) : 0;

		if (song.drum)
		{
			song.drum--;
			noise = noise >> 1 ^ (-(noise & 1) & 0xb400u);
			out[i] += noise & 1 ? AMPLITUDE / 4 : -AMPLITUDE / 4;
		}
	}
}


void DMX_PlaySound(const uint8_t* lump, uint8_t volume)
{
	SDL_LockAudioDevice(audio);
	sound.count = word(lump + 2);
	sound.tones = sound.count ? lump + 4 : NULL;
	sound.at = 0;
	sound.samples = 0;
	sound.amplitude = AMPLITUDE * volume / 120;
	SDL_UnlockAudioDevice(audio);
}


void DMX_PlaySong(const uint8_t* lump, boolean looping)
{
	SDL_LockAudioDevice(audio);
	song.count = lump ? word(lump) : 0;
	song.pairs = song.count ? lump : NULL;
	song.at = 0;
	song.looping = looping;
	song.tone = 0;
	song.samples = 0;
	song.drum = 0;
	SDL_UnlockAudioDevice(audio);
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
	static uint32_t pixels[SCREEN_HEIGHT][SCREEN_WIDTH];

	for (int y = 0; y < SCREEN_HEIGHT; y++)
	{
		for (int x = 0; x < SCREEN_WIDTH; x++)
		{
			const uint8_t* rgb = &palette[screen[y][x] * 3];
			pixels[y][x] = (uint32_t)rgb[0] << 16 | rgb[1] << 8 | rgb[2];
		}
	}

	SDL_UpdateTexture(texture, NULL, pixels, sizeof(pixels[0]));
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);

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
// The host
//

static const uint8_t* load(const char* path)
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
	return data;
}


int main(int argc, char** argv)
{
	int scale = 4;

	if (argc == 4 && !strcmp(argv[2], "-scale"))
		scale = atoi(argv[3]);
	else if (argc != 2)
	{
		fprintf(stderr, "usage: host WAD [-scale N]\n");
		return 2;
	}

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
		I_Error("SDL_Init: %s", SDL_GetError());

	window = SDL_CreateWindow("femto-doom", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale, SDL_WINDOW_RESIZABLE);
	renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC) : NULL;
	texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING,
		SCREEN_WIDTH, SCREEN_HEIGHT) : NULL;
	if (!texture)
		I_Error("SDL: %s", SDL_GetError());
	SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

	SDL_AudioSpec want = {0};
	want.freq = RATE;
	want.format = AUDIO_S16SYS;
	want.channels = 1;
	want.samples = 512;
	want.callback = mix;
	audio = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
	if (audio)
		SDL_PauseAudioDevice(audio, 0);

	W_SetWad(load(argv[1]));
	start_ms = SDL_GetTicks();
	D_DoomInit(1, (const char* const[]){"femto-doom"});

	for (;;)
		D_DoomStep();
}
