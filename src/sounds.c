/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *  Copyright 2023-2025 by
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
 *      Created by a sound utility.
 *      Kept as a sample, DOOM2 sounds.
 *
 *-----------------------------------------------------------------------------*/

#include <stddef.h>

#include "doomtype.h"
#include "sounds.h"

//
// Information about all the sfx
//

const sfxinfo_t S_sfx[] = {
  { 0 }, // none   S_sfx[0] needs to be a dummy for odd reasons.
  { 64 }, // pistol
  { 64 }, // shotgn
  { 64 }, // sgcock
  { 64 }, // sawup
  { 118 }, // sawidl
  { 64 }, // sawful
  { 64 }, // sawhit
  { 64 }, // rlaunc
  { 70 }, // rxplod
  { 70 }, // firsht
  { 70 }, // firxpl
  { 100 }, // pstart
  { 100 }, // pstop
  { 100 }, // doropn
  { 100 }, // dorcls
  { 119 }, // stnmov
  { 78 }, // swtchn
  { 78 }, // swtchx
  { 96 }, // plpain
  { 96 }, // dmpain
  { 96 }, // popain
  { 78 }, // slop
  { 78 }, // itemup
  { 78 }, // wpnup
  { 96 }, // oof
  { 32 }, // telept
  { 98 }, // posit1
  { 98 }, // posit2
  { 98 }, // posit3
  { 98 }, // bgsit1
  { 98 }, // bgsit2
  { 98 }, // sgtsit
  { 94 }, // brssit
  { 70 }, // sgtatk
  { 70 }, // claw
  { 32 }, // pldeth
  { 32 }, // pdiehi
  { 70 }, // podth1
  { 70 }, // podth2
  { 70 }, // podth3
  { 70 }, // bgdth1
  { 70 }, // bgdth2
  { 70 }, // sgtdth
  { 32 }, // brsdth
  { 120 }, // posact
  { 120 }, // bgact
  { 120 }, // dmact
  { 78 }, // noway
  { 60 }, // barexp
  { 64 }, // punch
  { 60 }, // tink 
  { 60 }, // getpow
};
