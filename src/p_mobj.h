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
 *      Map Objects, MObj, definition and handling.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __P_MOBJ__
#define __P_MOBJ__

// Basics.
#include <stddef.h>

#include "tables.h"
#include "m_fixed.h"

// We need the thinker_t stuff.
#include "d_think.h"

#include "z_zone.h"

// We need the WAD data structure for Map things,
// from the THINGS lump.
#include "doomdata.h"

// States are tied to finite states are
//  tied to animation frames.
// Needs precompiled tables/data structures.
#include "info.h"

//
// NOTES: mobj_t
//
// mobj_ts are used to tell the refresh where to draw an image,
// tell the world simulation when objects are contacted,
// and tell the sound driver how to position a sound.
//
// The refresh uses the next and prev links to follow
// lists of things in sectors as they are being drawn.
// The sprite, frame, and angle elements determine which patch_t
// is used to draw the sprite if it is visible.
// The sprite and frame values are allmost allways set
// from state_t structures.
// The statescr.exe utility generates the states.h and states.c
// files that contain the sprite/frame numbers from the
// statescr.txt source file.
// The xyz origin point represents a point at the bottom middle
// of the sprite (between the feet of a biped).
// This is the default origin position for patch_ts grabbed
// with lumpy.exe.
// A walking creature will have its z equal to the floor
// it is standing on.
//
// The sound code uses the x,y, and subsector fields
// to do stereo positioning of any sound effited by the mobj_t.
//
// The play simulation uses the blocklinks, x,y,z, radius, height
// to determine when mobj_ts are touching each other,
// touching lines in the map, or hit by trace lines (gunshots,
// lines of sight, etc).
// The mobj_t->flags element has various bit flags
// used by the simulation.
//
// Every mobj_t is linked into a single sector
// based on its origin coordinates.
// The subsector_t is found with R_PointInSubsector(x,y),
// and the sector_t can be found with subsector->sector.
// The sector links are only used by the rendering code,
// the play simulation does not care about them at all.
//
// Any mobj_t that needs to be acted upon by something else
// in the play world (block movement, be shot, etc) will also
// need to be linked into the blockmap.
// If the thing has the MF_NOBLOCK flag set, it will not use
// the block links. It can still interact with other things,
// but only as the instigator (missiles will run into other
// things, but nothing can run into a missile).
// Each block in the grid is 128*128 units, and knows about
// every line_t that it contains a piece of, and every
// interactable mobj_t that has its origin contained.
//
// A valid mobj_t is a mobj_t that has the proper subsector_t
// filled in for its xy coordinates and is linked into the
// sector from which the subsector was made, or has the
// MF_NOSECTOR flag set (the subsector_t needs to be valid
// even if MF_NOSECTOR is set), and is linked into a blockmap
// block or has the MF_NOBLOCKMAP flag set.
// Links should only be modified by the P_[Un]SetThingPosition()
// functions.
// Do not change the MF_NO? flags while a thing is valid.
//
// Any questions?
//

// The MF_ flags are in info.h.


// Map Object definition.
//
//
// killough 2/20/98:
//
// WARNING: Special steps must be taken in p_saveg.c if C pointers are added to
// this mobj_s struct, or else savegames will crash when loaded. See p_saveg.c.
// Do not add "struct mobj_s *fooptr" without adding code to p_saveg.c to
// convert the pointers to ordinals and back for savegames. This was the whole
// reason behind monsters going to sleep when loading savegames (the "target"
// pointer was simply nullified after loading, to prevent Doom from crashing),
// and the whole reason behind loadgames crashing on savegames of AV attacks.
//

// killough 9/8/98: changed some fields to shorts,
// for better memory usage (if only for cache).
/* cph 2006/08/28 - move Prev[XYZ] fields to the end of the struct. Add any
 * other new fields to the end, and make sure you don't break savegames! */

typedef struct mobj_s
{
    // Info for drawing: position.
    fixed_t             x;
    fixed_t             y;

    // More list: links in sector (if needed)
    zref_t              snext;  // mobj_t
    zref_t              sprev;  // the zref_t that points here

    int16_t             subsector;

    mobjtype_t          type;

    spritenum_t         sprite; // used to find patch_t and flip value

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    zref_t              bnext;  // mobj_t
    zref_t              bprev;  // the zref_t that points here

    fixed_t             z;

    //More drawing info: to determine current sprite.
    angle_t             angle;  // orientation

    // For movement checking the radius and height are the type's: the height a quarter on a
    // corpse, both 0 once gibbed.

    uint32_t            flags;

                                       // phares 3/17/98
    // a linked list of sectors where this object appears
    zref_t              touching_sectorlist;    // msecnode_t

    // The closest interval over all contacted Sectors, in quarter units as sector heights are.
    int16_t             floorz;
    int16_t             ceilingz;

    int16_t             state;  // index into states[]

    uint16_t            frame;  // might be ORed with FF_FULLBRIGHT

    int16_t             tics;   // state tic counter
} mobj_t;

typedef char assertMobjSize[sizeof(mobj_t) == 44 ? 1 : -1];

// A thing of type MT_MISC0 or later lives in the static pool as a bare
// mobj_t with MF_STATIC set. Every other thing is the mo of an mobjext_t:
// its thinker sits at offset 0 of the allocation, the thinker list links it
// and Z_Free releases it through that pointer.
typedef struct
{
    // List: thinker links.
    thinker_t           thinker;

    int16_t             health;

    // Momentums, used to update position.
    fixed_t             momx;
    fixed_t             momy;
    fixed_t             momz;

    // Thing being chased/attacked (or NULL),
    // also the originator for missiles.
    zref_t              target;     // mobj_t

    int16_t             movecount;      // when 0, select a new dir

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    int16_t             reactiontime;

    // Movement direction, movement generation (zig-zagging).

    uint8_t            movedir;

    // If >0, the current target will be chased no
    // matter what (even if shot by another object)
    uint8_t            threshold;

    // Load index + 1 of a map thing, 0 for any other.
    int16_t            loadthing;

    mobj_t              mo;
} mobjext_t;

typedef char assertMobjExtSize[sizeof(mobjext_t) == 76 ? 1 : -1];

// A form in the high bits of type marks a thing in the sector and block lists that is no
// mobj_t. Every form starts like a mobj_t up to its sprite, which a form uses for itself.
#define FORM_REST   0x40
#define FORM_EFFECT 0x80
#define FORM_MASK   0xc0

// A dropped item or a corpse at rest: only its sector changes it again, and z is its floorz.
typedef struct
{
    fixed_t             x;
    fixed_t             y;
    zref_t              snext;
    zref_t              sprev;
    int16_t             subsector;
    mobjtype_t          type;   // with FORM_REST
    uint8_t             bits;   // REST_*
    zref_t              bnext;
    zref_t              bprev;
    zref_t              touching_sectorlist;
    int16_t             floorz;
} restmobj_t;

#define REST_CORPSE     1
#define REST_XDEATH     2
#define REST_GIBS       4

typedef char assertRestSize[sizeof(restmobj_t) == 24 ? 1 : -1];
typedef char assertRestHead[offsetof(restmobj_t, snext) == offsetof(mobj_t, snext)
                            && offsetof(restmobj_t, sprev) == offsetof(mobj_t, sprev)
                            && offsetof(restmobj_t, subsector) == offsetof(mobj_t, subsector)
                            && offsetof(restmobj_t, type) == offsetof(mobj_t, type)
                            && offsetof(restmobj_t, bnext) == offsetof(mobj_t, bnext)
                            && offsetof(restmobj_t, bprev) == offsetof(mobj_t, bprev) ? 1 : -1];

// A puff, blood or teleport fog: it has no block links, and of its momentum only momz.
typedef struct
{
    fixed_t             x;
    fixed_t             y;
    zref_t              snext;
    zref_t              sprev;
    int16_t             subsector;
    mobjtype_t          type;   // with FORM_EFFECT
    int8_t              tics;
    fixed_t             z;
    zref_t              touching_sectorlist;
    int16_t             floorz;
    int16_t             ceilingz;
    int16_t             state;
    int8_t              momz;   // in FRACUNIT
    thinker_t           thinker;
} effectmobj_t;

typedef char assertEffectSize[sizeof(effectmobj_t) == 36 ? 1 : -1];
typedef char assertEffectHead[offsetof(effectmobj_t, snext) == offsetof(mobj_t, snext)
                              && offsetof(effectmobj_t, sprev) == offsetof(mobj_t, sprev)
                              && offsetof(effectmobj_t, subsector) == offsetof(mobj_t, subsector)
                              && offsetof(effectmobj_t, type) == offsetof(mobj_t, type) ? 1 : -1];

// The mobj_t a visit to a resting thing works on, with MF_REST set.
typedef struct
{
    mobj_t              mo;
    restmobj_t*         rest;
    zref_t              links[5];
} restvisit_t;

static inline mobj_t* P_Mobj(zref_t ref)
{
    return Z_Ptr(ref);
}

static inline uint8_t P_Form(const mobj_t* mobj)
{
    return (uint8_t)mobj->type & FORM_MASK;
}

// The thinking mobj an effect is while it spawns or thinks.
typedef struct
{
    mobjext_t           ext;
    effectmobj_t*       effect;
} effectvisit_t;

static inline const restmobj_t* P_RestOf(const mobj_t* mobj)
{
    return ((const restvisit_t*)mobj)->rest;
}



// External declarations (fomerly in p_local.h) -- killough 5/2/98

#define VIEWHEIGHT      (41*FRACUNIT)


#define ONFLOORZ        INT32_MIN
#define ONCEILINGZ      INT32_MAX


mobj_t* P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);
void    P_RemoveMobj(mobj_t* th);
boolean P_SetMobjState(mobj_t* mobj, statenum_t state);

void    P_MobjThinker(thinker_t* th);
void    P_MobjBrainlessThinker(thinker_t* th);
mobj_t* P_NewMobj(void);

mobjext_t* P_Ext(mobj_t* mobj);
int16_t P_Health(mobj_t* mobj);
mobj_t* P_MobjOfThinker(thinker_t* th);
fixed_t PUREFUNC P_Radius(const mobj_t* mobj);
fixed_t PUREFUNC P_Height(const mobj_t* mobj);
int16_t P_ToQuarters(fixed_t z);

static inline fixed_t P_FromQuarters(int16_t z)
{
    return (fixed_t)z * (FRACUNIT >> 2);
}
void    P_RunStatics(void);

void    P_SpawnDrop(fixed_t x, fixed_t y, mobjtype_t type);
void    P_RestMobj(restmobj_t* rest, restvisit_t* visit);
void    P_RestVisited(restvisit_t* visit);
boolean P_VisitRest(mobj_t* rest, boolean func(mobj_t*));
void    P_BeginEffect(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type, effectvisit_t* visit);
void    P_EndEffect(effectvisit_t* visit);
mobj_t* P_SpawnFog(fixed_t x, fixed_t y, fixed_t z);
void    P_EffectMobj(const effectmobj_t* effect, mobj_t* mobj);
void    P_EffectThinker(thinker_t* th);
void    P_RemoveEffectDelayed(thinker_t* th);

void    P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z);
void    P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, int16_t damage);
mobj_t* P_SpawnMissile(mobj_t* source, mobj_t* dest, mobjtype_t type);
void    P_SpawnPlayerMissile(mobj_t* source);
boolean P_MapThingSpawns(const mapthing_t* mthing);
boolean P_MapThingIsStatic(const mapthing_t* mthing);
void    P_SpawnMapThing (const mapthing_t* mthing);

struct player_s* P_MobjIsPlayer(const mobj_t* mobj);

#endif

