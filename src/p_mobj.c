/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2004 by
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
 *      Moving object handling. Spawn functions.
 *
 *-----------------------------------------------------------------------------*/

#include <stdint.h>

#include "doomdef.h"
#include "d_player.h"
#include "m_random.h"
#include "r_main.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_tick.h"
#include "sounds.h"
#include "s_sound.h"
#include "info.h"
#include "g_game.h"
#include "p_inter.h"
#include "i_platform.h"

#include "globdata.h"


//
// P_SetMobjState
// Returns true if the mobj is still present.
//

boolean P_SetMobjState(mobj_t* mobj, statenum_t state)
{
    const state_t*	st;

    do
    {
        if (state == S_NULL)
        {
            mobj->state = S_NULL;
            P_RemoveMobj (mobj);
            return false;
        }

        st = &states[state];
        mobj->state = state;
        mobj->tics = st->tics;
        if (_g_gameskill == sk_nightmare && state >= S_SARG_RUN1 && state <= S_SARG_PAIN2)
            mobj->tics >>= 1;  // fast demons
        mobj->sprite = st->sprite;
        mobj->frame = st->frame;

        // Modified handling.
        // Call action functions when the state is set
        if(st->action)
        {
            ((void (*)(mobj_t*))stateactions[st->action])(mobj);
        }

        state = st->nextstate;

    } while (!mobj->tics);

    return true;
}


//
// P_ExplodeMissile
//

static void P_ExplodeMissile(mobj_t* mo)
  {
  P_Ext(mo)->momx = P_Ext(mo)->momy = P_Ext(mo)->momz = 0;

  P_SetMobjState (mo, mobjinfo[mo->type].deathstate);

  mo->tics -= P_Random()&3;

  if (mo->tics < 1)
    mo->tics = 1;

  mo->flags &= ~MF_MISSILE;

  if (mobjinfo[mo->type].deathsound)
    S_StartSound (mo, mobjinfo[mo->type].deathsound);
  }


//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//

static fixed_t   bestslidefrac;
static const line_t*   bestslideline;
static mobj_t*   slidemo;
static fixed_t   tmxmove;
static fixed_t   tmymove;

//
// P_HitSlideLine
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
// If the floor is icy, then you can bounce off a wall.             // phares
//

static void P_HitSlideLine(const line_t* ld)
{
    int16_t     side;
    angle_t lineangle;
    angle_t moveangle;
    angle_t deltaangle;
    fixed_t movelen;
    fixed_t newlen;
    //   |
    // Under icy conditions, if the angle of approach to the wall     //   V
    // is more than 45 degrees, then you'll bounce and lose half
    // your momentum. If less than 45 degrees, you'll slide along
    // the wall. 45 is arbitrary and is believable.

    // Check for the special cases of horz or vert walls.

    /* killough 10/98: only bounce if hit hard (prevents wobbling)
   * cph - DEMOSYNC - should only affect players in Boom demos? */

    line_t l;
    W_Fetch(ld, &l, sizeof(l));

    if (l.dy == 0)
    {
        tmymove = 0; // no more movement in the Y direction
        return;
    }

    if (l.dx == 0)
    {                                                          // phares
        tmxmove = 0; // no more movement in the X direction
        return;
    }

    // The wall is angled. Bounce if the angle of approach is         // phares
    // less than 45 degrees.                                          // phares

    side = P_PointOnLineSide (slidemo->x, slidemo->y, ld);

    lineangle = R_PointToAngle2 (0,0, (fixed_t)l.dx<<FRACBITS, (fixed_t)l.dy<<FRACBITS);
    if (side == 1)
        lineangle += ANG180;

    moveangle = R_PointToAngle2 (0,0, tmxmove, tmymove);
    deltaangle = moveangle-lineangle;
    movelen = P_AproxDistance (tmxmove, tmymove);
    //   |
    // phares
    if (deltaangle > ANG180)
        deltaangle += ANG180;

    //  I_Error ("SlideLine: ang>ANG180");

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;
    newlen  = FixedMulAngle(movelen, finecosine(deltaangle));
    tmxmove = FixedMulAngle(newlen,  finecosine(lineangle));
    tmymove = FixedMulAngle(newlen,  finesine(  lineangle));
    // phares
}


//
// PTR_SlideTraverse
//

static boolean PTR_SlideTraverse (intercept_t* in)
  {
  const line_t* li;

  if (!in->isaline)
    I_Error ("PTR_SlideTraverse: not a line?");

  li = in->d.line;

  if ( ! (LN_FLAGS(li) & ML_TWOSIDED) )
    {
    if (P_PointOnLineSide (slidemo->x, slidemo->y, li))
      return true; // don't hit the back side
    goto isblocking;
    }

  // set openrange, opentop, openbottom.
  // These define a 'window' from one sector to another across a line

  P_LineOpening (li);

  if (_g_openrange < P_Height(slidemo))
    goto isblocking;  // doesn't fit

  if (_g_opentop - slidemo->z < P_Height(slidemo))
    goto isblocking;  // mobj is too high

  if (_g_openbottom - slidemo->z > 24*FRACUNIT )
    goto isblocking;  // too big a step up

  // this line doesn't block movement

  return true;

  // the line does block movement,
  // see if it is closer than best so far

isblocking:

  if (in->frac < bestslidefrac)
    {
    bestslidefrac = in->frac;
    bestslideline = li;
    }

  return false; // stop
  }


//
// P_SlideMove
// The momx / momy move is bad, so try to slide
// along a wall.
// Find the first line hit, move flush to it,
// and slide along it
//
// This is a kludgy mess.
//
// killough 11/98: reformatted

static void P_SlideMove(mobj_t* mo)
{
  int8_t hitcount = 3;

  slidemo = mo; // the object that's sliding

  do
    {
      fixed_t leadx, leady, trailx, traily;

      if (!--hitcount)
  goto stairstep;   // don't loop forever

      // trace along the three leading corners

      if (P_Ext(mo)->momx > 0)
  leadx = mo->x + P_Radius(mo), trailx = mo->x - P_Radius(mo);
      else
  leadx = mo->x - P_Radius(mo), trailx = mo->x + P_Radius(mo);

      if (P_Ext(mo)->momy > 0)
  leady = mo->y + P_Radius(mo), traily = mo->y - P_Radius(mo);
      else
  leady = mo->y - P_Radius(mo), traily = mo->y + P_Radius(mo);

      bestslidefrac = FRACUNIT+1;

      P_PathTraverse(leadx, leady, leadx+P_Ext(mo)->momx, leady+P_Ext(mo)->momy,
         PT_ADDLINES, PTR_SlideTraverse);
      P_PathTraverse(trailx, leady, trailx+P_Ext(mo)->momx, leady+P_Ext(mo)->momy,
         PT_ADDLINES, PTR_SlideTraverse);
      P_PathTraverse(leadx, traily, leadx+P_Ext(mo)->momx, traily+P_Ext(mo)->momy,
         PT_ADDLINES, PTR_SlideTraverse);

      // move up to the wall

      if (bestslidefrac == FRACUNIT+1)
  {
    // the move must have hit the middle, so stairstep

  stairstep:

    /* phares 5/4/98: kill momentum if you can't move at all
     * This eliminates player bobbing if pressed against a wall
     * while on ice.
     *
     * killough 10/98: keep buggy code around for old Boom demos
     *
     * cph 2000/09//23: buggy code was only in Boom v2.01
     */

    if (!P_TryMove(mo, mo->x, mo->y + P_Ext(mo)->momy))
      P_TryMove(mo, mo->x + P_Ext(mo)->momx, mo->y);


    break;
  }

      // fudge a bit to make sure it doesn't hit

      if ((bestslidefrac -= 0x800) > 0)
  {
    fixed_t newx = FixedMul(P_Ext(mo)->momx, bestslidefrac);
    fixed_t newy = FixedMul(P_Ext(mo)->momy, bestslidefrac);

    if (!P_TryMove(mo, mo->x+newx, mo->y+newy))
      goto stairstep;
  }

      // Now continue along the wall.
      // First calculate remainder.

      bestslidefrac = FRACUNIT-(bestslidefrac+0x800);

      if (bestslidefrac > FRACUNIT)
            bestslidefrac = FRACUNIT;

      if (bestslidefrac <= 0)
  break;

      tmxmove = FixedMul(P_Ext(mo)->momx, bestslidefrac);
      tmymove = FixedMul(P_Ext(mo)->momy, bestslidefrac);

      P_HitSlideLine(bestslideline); // clip the moves

      P_Ext(mo)->momx = tmxmove;
      P_Ext(mo)->momy = tmymove;

    }
  while (!P_TryMove(mo, mo->x+tmxmove, mo->y+tmymove));
}


static fixed_t FixedMul32OrigFriction(fixed_t a)
{
	uint16_t alw = a;
	 int16_t ahw = a >> FRACBITS;

	uint32_t ll = (uint32_t) alw * ORIG_FRICTION;
	 int32_t hl = ( int32_t) ahw * ORIG_FRICTION;
	return (ll >> FRACBITS) + hl;
}


//
// P_XYMovement
//
// Attempts to move something if it has momentum.
//

#define MAXMOVE         (30*FRACUNIT)
#define STOPSPEED       (FRACUNIT/16)

static void P_XYMovement(mobj_t* mo)
{
    player_t *player;
    fixed_t xmove, ymove;

    if (!(P_Ext(mo)->momx | P_Ext(mo)->momy)) // Any momentum?
    {
        return;
    }

    player = P_MobjIsPlayer(mo);

    if (P_Ext(mo)->momx > MAXMOVE)
        P_Ext(mo)->momx = MAXMOVE;
    else if (P_Ext(mo)->momx < -MAXMOVE)
        P_Ext(mo)->momx = -MAXMOVE;

    if (P_Ext(mo)->momy > MAXMOVE)
        P_Ext(mo)->momy = MAXMOVE;
    else if (P_Ext(mo)->momy < -MAXMOVE)
        P_Ext(mo)->momy = -MAXMOVE;

    xmove = P_Ext(mo)->momx;
    ymove = P_Ext(mo)->momy;

    do
    {
        fixed_t ptryx, ptryy;

        if (xmove > MAXMOVE/2 || ymove > MAXMOVE/2)
        {
            ptryx = mo->x + xmove/2;
            ptryy = mo->y + ymove/2;
            xmove >>= 1;
            ymove >>= 1;
        }
        else
        {
            ptryx = mo->x + xmove;
            ptryy = mo->y + ymove;
            xmove = ymove = 0;
        }

        if (!P_TryMove (mo, ptryx, ptryy))
        {
            // blocked move

            if (player)   // try to slide along it
                P_SlideMove (mo);
            else
            {
                if (mo->flags & MF_MISSILE)
                {
                    // explode a missile

                    if (_g_ceilingline)
                    {
                        const sector_t* ceilingBackSector = LN_BACKSECTOR(_g_ceilingline);

                        if(ceilingBackSector && SEC_CEILINGSKY(ceilingBackSector))
                        {
                            // Hack to prevent missiles exploding
                            // against the sky.
                            // Does not handle sky floors.

                            P_RemoveMobj (mo);
                            return;
                        }
                    }

                    P_ExplodeMissile (mo);
                }
                else // whatever else it is, it is now standing still in (x,y)
                {
                    P_Ext(mo)->momx = P_Ext(mo)->momy = 0;
                }
            }
        }
    } while (xmove || ymove);

    // slow down

    /* no friction for missiles or skulls ever, no friction when airborne */
    if (mo->flags & MF_MISSILE || mo->z > P_FromQuarters(mo->floorz))
        return;

    /* killough 8/11/98: add bouncers
   * killough 9/15/98: add objects falling off ledges
   * killough 11/98: only include bouncers hanging off ledges
   */
    if ((mo->flags & MF_CORPSE) &&
            (P_Ext(mo)->momx > FRACUNIT/4 || P_Ext(mo)->momx < -FRACUNIT/4 ||
             P_Ext(mo)->momy > FRACUNIT/4 || P_Ext(mo)->momy < -FRACUNIT/4) &&
            P_FromQuarters(mo->floorz) != SEC_FLOORHEIGHT(P_SubsectorSector(mo->subsector)))
        return;  // do not stop sliding if halfway off a step with some momentum

    // killough 11/98:
    // Stop voodoo dolls that have come to rest, despite any
    // moving corresponding player, except in old demos:

    if (P_Ext(mo)->momx > -STOPSPEED && P_Ext(mo)->momx < STOPSPEED &&
            P_Ext(mo)->momy > -STOPSPEED && P_Ext(mo)->momy < STOPSPEED &&
            (!player || !(player->cmd.forwardmove | player->cmd.sidemove) ||
             (player->mo != mo)))
    {
        // if in a walking frame, stop moving

        // killough 10/98:
        // Don't affect main player when voodoo dolls stop, except in old demos:

        if (player && (uint16_t)(player->mo->state - S_PLAY_RUN1) < 4
                && (player->mo == mo))
            P_SetMobjState(player->mo, S_PLAY);

        P_Ext(mo)->momx = P_Ext(mo)->momy = 0;
    }
    else
    {
        /* phares 3/17/98
       *
       * Friction will have been adjusted by friction thinkers for
       * icy or muddy floors. Otherwise it was never touched and
       * remained set at ORIG_FRICTION
       *
       * killough 8/28/98: removed inefficient thinker algorithm,
       * instead using touching_sectorlist in P_GetFriction() to
       * determine friction (and thus only when it is needed).
       *
       * killough 10/98: changed to work with new bobbing method.
       * Reducing player momentum is no longer needed to reduce
       * bobbing, so ice works much better now.
       *
       * cph - DEMOSYNC - need old code for Boom demos?
       */

        P_Ext(mo)->momx = FixedMul32OrigFriction(P_Ext(mo)->momx);
        P_Ext(mo)->momy = FixedMul32OrigFriction(P_Ext(mo)->momy);

        /* killough 10/98: Always decrease player bobbing by ORIG_FRICTION.
       * This prevents problems with bobbing on ice, where it was not being
       * reduced fast enough, leading to all sorts of kludges being developed.
       */

    }
}


//
// P_ZMovement
//
// Attempt vertical movement.

#define GRAVITY         FRACUNIT

static void P_ZMovement(mobj_t* mo)
{

    // check for smooth step up

    if (P_MobjIsPlayer(mo) &&
            P_MobjIsPlayer(mo)->mo == mo &&  // killough 5/12/98: exclude voodoo dolls
            mo->z < P_FromQuarters(mo->floorz))
    {
        P_MobjIsPlayer(mo)->viewheight -= P_FromQuarters(mo->floorz)-mo->z;
        P_MobjIsPlayer(mo)->deltaviewheight = (VIEWHEIGHT - P_MobjIsPlayer(mo)->viewheight)>>3;
    }

  // adjust altitude

  mo->z += P_Ext(mo)->momz;

  // clip movement

  if (mo->z <= P_FromQuarters(mo->floorz))
    {
    // hit the floor

    if (P_Ext(mo)->momz < 0)
      {
    if (P_MobjIsPlayer(mo) && /* killough 5/12/98: exclude voodoo dolls */
        P_MobjIsPlayer(mo)->mo == mo && P_Ext(mo)->momz < -GRAVITY*8)
      {
        // Squat down.
        // Decrease viewheight for a moment
        // after hitting the ground (hard),
        // and utter appropriate sound.

        P_MobjIsPlayer(mo)->deltaviewheight = P_Ext(mo)->momz>>3;
        if (P_Ext(mo)->health > 0) /* cph - prevent "oof" when dead */
    S_StartSound (mo, sfx_oof);
      }
  P_Ext(mo)->momz = 0;
      }
    mo->z = P_FromQuarters(mo->floorz);

    if ( (mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP) )
      {
      P_ExplodeMissile (mo);
      return;
      }
    }
  else // still above the floor                                     // phares
    if (!(mo->flags & MF_NOGRAVITY))
      {
  if (!P_Ext(mo)->momz)
    P_Ext(mo)->momz = -GRAVITY;
        P_Ext(mo)->momz -= GRAVITY;
      }

  if (mo->z + P_Height(mo) > P_FromQuarters(mo->ceilingz))
    {
    // hit the ceiling

    if (P_Ext(mo)->momz > 0)
      P_Ext(mo)->momz = 0;

    mo->z = P_FromQuarters(mo->ceilingz) - P_Height(mo);

    if ( (mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP) )
      {
      P_ExplodeMissile (mo);
      return;
      }
    }
  }

//
// P_NightmareRespawn
//

static void P_NightmareRespawn(mobj_t* mobj)
{
    fixed_t      x;
    fixed_t      y;
    angle_t      angle;
    boolean      ambush;
    int16_t ss;
    mobj_t*      mo;

    // Every monster is a map thing, whose spot it returns to.
    const int16_t loadthing = P_Ext(mobj)->loadthing;
    if (!loadthing)
        return;
    P_LoadThingSpot(loadthing - 1, &x, &y, &angle, &ambush);

    // something is occupying its position?

    if (!P_CheckPosition (mobj, x, y) )
        return; // no respwan

    // spawn a teleport fog at old spot
    // because of removal of the body?

    mo = P_SpawnFog (mobj->x,
                     mobj->y,
                     SEC_FLOORHEIGHT(P_SubsectorSector(mobj->subsector)));

    // initiate teleport sound

    S_StartSound (mo, sfx_telept);

    // spawn a teleport fog at the new spot

    ss = R_PointInSubsector (x,y);

    mo = P_SpawnFog (x, y, SEC_FLOORHEIGHT(P_SubsectorSector(ss)));

    S_StartSound (mo, sfx_telept);

    // spawn the new monster

    mo = P_SpawnMobj(x, y, ONFLOORZ, mobj->type);
    mo->angle = angle;
    mo->flags |= MF_RESPAWNED | (ambush ? MF_AMBUSH : 0);
    P_Ext(mo)->loadthing = loadthing;
    P_Ext(mo)->reactiontime = 18;

    // remove the old monster,

    P_RemoveMobj (mobj);
}


mobjext_t* P_Ext(mobj_t* mobj)
{
    if (mobj->flags & MF_STATIC)
        I_Error("P_Ext: static thing %i", mobj->type);

    return (mobjext_t*)((uint8_t*)mobj - offsetof(mobjext_t, mo));
}


mobj_t* P_MobjOfThinker(thinker_t* th)
{
    return &((mobjext_t*)th)->mo;
}


fixed_t PUREFUNC P_Radius(const mobj_t* mobj)
{
    return (mobj->flags & MF_CORPSE) && mobj->state == S_GIBS ? 0 : mobjinfo[mobj->type].radius;
}


fixed_t PUREFUNC P_Height(const mobj_t* mobj)
{
    if (!(mobj->flags & MF_CORPSE))
        return mobjinfo[mobj->type].height;

    return mobj->state == S_GIBS ? 0 : mobjinfo[mobj->type].height >> 2;
}


int16_t P_ToQuarters(fixed_t z)
{
    const fixed_t quarters = z / (FRACUNIT >> 2);
    if (quarters * (FRACUNIT >> 2) != z || quarters != (int16_t)quarters)
        I_Error("P_ToQuarters: %li", (long)z);

    return quarters;
}


int16_t P_Health(mobj_t* mobj)
{
    return mobj->flags & MF_STATIC ? 0 : P_Ext(mobj)->health;
}

static restmobj_t* P_RestCorpse(const mobj_t* mobj, zref_t ref);
static void P_InitMobj(mobj_t* mobj, fixed_t x, fixed_t y, fixed_t z, mobjtype_t type, void* at);

// A corpse at rest only ever changes again through its sector, so it goes on as a static:
// the same mobj_t in a block of its own, and everything that referred to it refers to that.
// Unless something does, it rests in a resting thing instead.
static void P_SettleCorpse(mobjext_t* ext)
{
    mobj_t* old = &ext->mo;
    const zref_t from = Z_Ref(old);
    mobj_t* at = (mobj_t*)P_RestCorpse(old, from);
    if (!at)
    {
        at = Z_MallocLevel(sizeof(mobj_t));
        *at = *old;
        at->flags = (old->flags & ~MF_POOLED) | MF_STATIC;
    }
    const zref_t to = Z_Ref(at);

    if (!(old->flags & MF_INPLACE))
    {
        if (!(old->flags & MF_NOSECTOR))
        {
            *(zref_t*)Z_Ptr(old->sprev) = to;
            if (old->snext)
                P_Mobj(old->snext)->sprev = Z_Ref(&at->snext);
        }
        if (!(old->flags & MF_NOBLOCKMAP) && old->bprev)
        {
            *(zref_t*)Z_Ptr(old->bprev) = to;
            if (old->bnext)
                P_Mobj(old->bnext)->bprev = Z_Ref(&at->bnext);
        }
    }
    for (msecnode_t* n = P_Node(old->touching_sectorlist); n; n = P_Node(n->m_tnext))
        n->m_thing = to;
    P_MoveLoadRef(ext->loadthing, at);

    for (thinker_t* th = P_NextThinker(NULL); th; th = P_NextThinker(th))
        if (th->function == TH_MOBJ)
        {
            mobjext_t* e = (mobjext_t*)th;
            if (e->target == from)
                e->target = to;
        }
    if (_g_player.attacker == old)
        _g_player.attacker = at;
    if (_g_linetarget == old)
        _g_linetarget = at;

    P_RemoveThing(old);
}

void P_MobjThinker (thinker_t* th)
{
    mobjext_t* ext = (mobjext_t*)th;
    mobj_t* mobj = &ext->mo;

    // momentum movement
    if (ext->momx | ext->momy)
    {
        P_XYMovement(mobj);
        if (th->function != TH_MOBJ) // cph - Must've been removed
            return;       // killough - mobj was removed
    }

    if (mobj->z != P_FromQuarters(mobj->floorz) || ext->momz)
    {
        P_ZMovement(mobj);
        if (th->function != TH_MOBJ) // cph - Must've been removed
            return;       // killough - mobj was removed
    }

    // cycle through states,
    // calling action functions at transitions

    if (mobj->tics != -1)
    {
        mobj->tics--;

        // you can cycle through multiple states in a tic

        if (!mobj->tics)
            if (!P_SetMobjState (mobj, states[mobj->state].nextstate) )
                return;     // freed itself
    }
    else
    {

        // check for nightmare respawn

        if (! (mobj->flags & MF_COUNTKILL) )
            return;

        if (!_g_respawnmonsters)
        {
            if ((mobj->flags & MF_CORPSE) && !ext->momz && mobj->z == P_FromQuarters(mobj->floorz) && !(ext->momx | ext->momy))
                P_SettleCorpse(ext);
            return;
        }

        ext->movecount++;

        if (ext->movecount < 12*35)
            return;

        if (_g_leveltime & 31)
            return;

        if (P_Random () > 4)
            return;

        P_NightmareRespawn (mobj);
    }

}


//Just cycles through the states. Allows sprite animation to work.
static void P_CycleState(mobj_t* mobj)
{
    // cycle through states,
    // calling action functions at transitions

    if (mobj->tics != -1)
    {
        mobj->tics--;

        // you can cycle through multiple states in a tic

        if (!mobj->tics)
            P_SetMobjState (mobj, states[mobj->state].nextstate);
    }
}


//Thinker function for stuff that doesn't need to do anything
//interesting.
void P_MobjBrainlessThinker(thinker_t* th)
{
    P_CycleState(P_MobjOfThinker(th));
}


void P_RunStatics(void)
{
    while (_g_removedrests)
    {
        restmobj_t* rest = Z_Ptr(_g_removedrests);
        _g_removedrests = rest->touching_sectorlist;
        Z_Free(rest);
    }

    P_RunLoadStatics();
}



static restmobj_t* P_NewRest(void)
{
    return Z_MallocLevel(sizeof(restmobj_t));
}

static void P_RestLinks(const restmobj_t* rest, zref_t* links)
{
    links[0] = rest->snext;
    links[1] = rest->sprev;
    links[2] = rest->bnext;
    links[3] = rest->bprev;
    links[4] = rest->touching_sectorlist;
}

// What P_SpawnMobj and P_KillMobj make of an item a monster drops, kept as a resting thing.
void P_SpawnDrop(fixed_t x, fixed_t y, mobjtype_t type)
{
    if (states[mobjinfo[type].spawnstate].tics != -1 || (mobjinfo[type].flags & (MF_NOSECTOR | MF_NOBLOCKMAP)))
        I_Error("P_SpawnDrop: %i does not rest", type);

    restmobj_t* rest = P_NewRest();
    mobj_t mobj;
    memset(&mobj, 0, sizeof(mobj));
    mobj.flags = MF_STATIC;
    P_InitMobj(&mobj, x, y, ONFLOORZ, type, rest);

    rest->x         = x;
    rest->y         = y;
    rest->snext     = mobj.snext;
    rest->sprev     = mobj.sprev;
    rest->subsector = mobj.subsector;
    rest->type      = type | FORM_REST;
    rest->bits      = 0;
    rest->bnext     = mobj.bnext;
    rest->bprev     = mobj.bprev;
    rest->touching_sectorlist = mobj.touching_sectorlist;
    rest->floorz    = mobj.floorz;
}

static int16_t P_FinalState(int16_t state)
{
    for (int16_t k = 0; states[state].tics != -1; k++)
    {
        if (k == 32)
            I_Error("P_FinalState: %i does not end", state);
        state = states[state].nextstate;
    }
    return state;
}

// The flags P_KillMobj and A_Fall leave a monster with, but those only its own thinking reads.
static uint32_t P_CorpseFlags(const mobjinfo_t* info)
{
    return (info->flags & ~(MF_SHOOTABLE | MF_NOGRAVITY | MF_SOLID)) | MF_CORPSE | MF_DROPOFF;
}

// The flags and state of a resting thing.
static void P_RestForm(const restmobj_t* rest, uint32_t* flags, int16_t* state)
{
    const mobjinfo_t* info = &mobjinfo[rest->type & ~FORM_MASK];
    if (!(rest->bits & REST_CORPSE))
    {
        *flags = info->flags | MF_STATIC | MF_DROPPED | MF_REST;
        *state = info->spawnstate;
        return;
    }
    *flags = P_CorpseFlags(info) | MF_STATIC | MF_REST;
    *state = rest->bits & REST_GIBS ? S_GIBS
           : P_FinalState(rest->bits & REST_XDEATH ? info->xdeathstate : info->deathstate);
}

// A resting thing for a settled corpse that nothing refers to, if its flags and state are
// those P_RestForm gives it; else NULL.
static restmobj_t* P_RestCorpse(const mobj_t* mobj, zref_t ref)
{
    const mobjinfo_t* info = &mobjinfo[mobj->type];
    uint8_t bits;
    if ((mobj->flags & ~(MF_POOLED | MF_AMBUSH | MF_JUSTHIT | MF_JUSTATTACKED | MF_LOOKLATE)) != P_CorpseFlags(info))
        return NULL;
    if (mobj->state == S_GIBS)
        bits = REST_CORPSE | REST_GIBS;
    else if (mobj->state == P_FinalState(info->deathstate))
        bits = REST_CORPSE;
    else if (info->xdeathstate && mobj->state == P_FinalState(info->xdeathstate))
        bits = REST_CORPSE | REST_XDEATH;
    else
        return NULL;

    if (_g_player.attacker == mobj || _g_linetarget == mobj)
        return NULL;
    for (thinker_t* th = P_NextThinker(NULL); th; th = P_NextThinker(th))
        if (th->function == TH_MOBJ && ((mobjext_t*)th)->target == ref)
            return NULL;

    restmobj_t* rest = P_NewRest();
    rest->x         = mobj->x;
    rest->y         = mobj->y;
    rest->snext     = mobj->snext;
    rest->sprev     = mobj->sprev;
    rest->subsector = mobj->subsector;
    rest->type      = mobj->type | FORM_REST;
    rest->bits      = bits;
    rest->bnext     = mobj->bnext;
    rest->bprev     = mobj->bprev;
    rest->touching_sectorlist = mobj->touching_sectorlist;
    rest->floorz    = mobj->floorz;
    return rest;
}

void P_RestMobj(restmobj_t* rest, restvisit_t* visit)
{
    const mobjtype_t type = rest->type & ~FORM_MASK;
    uint32_t flags;
    int16_t state;
    P_RestForm(rest, &flags, &state);
    const state_t* st = &states[state];
    mobj_t* mobj = &visit->mo;
    memset(mobj, 0, sizeof(*mobj));
    mobj->x         = rest->x;
    mobj->y         = rest->y;
    mobj->snext     = rest->snext;
    mobj->sprev     = rest->sprev;
    mobj->subsector = rest->subsector;
    mobj->type      = type;
    mobj->sprite    = st->sprite;
    mobj->bnext     = rest->bnext;
    mobj->bprev     = rest->bprev;
    mobj->z         = P_FromQuarters(rest->floorz);
    mobj->flags     = flags;
    mobj->touching_sectorlist = rest->touching_sectorlist;
    mobj->floorz    = rest->floorz;
    mobj->state     = state;
    mobj->frame     = st->frame;
    mobj->tics      = st->tics;
    visit->rest     = rest;
    P_RestLinks(rest, visit->links);
}

// Takes back what a visit changed: a resting thing only gets new heights, turns to gibs or goes.
void P_RestVisited(restvisit_t* visit)
{
    restmobj_t* rest = visit->rest;
    const mobj_t* mobj = &visit->mo;
    zref_t links[5];
    uint32_t flags;
    int16_t state;
    P_RestLinks(rest, links);
    if ((rest->bits & REST_CORPSE) && mobj->state == S_GIBS)
        rest->bits |= REST_GIBS;
    P_RestForm(rest, &flags, &state);
    if (memcmp(links, visit->links, sizeof(links)) || mobj->x != rest->x || mobj->y != rest->y
        || mobj->z != P_FromQuarters(mobj->floorz) || mobj->state != state || (mobj->flags & ~MF_REMOVED) != flags)
        I_Error("P_RestVisited: %i moved", mobj->type);
    rest->floorz = mobj->floorz;
    if (mobj->flags & MF_REMOVED)
    {
        rest->touching_sectorlist = _g_removedrests;
        _g_removedrests = Z_Ref(rest);
    }
}

boolean P_VisitRest(mobj_t* thing, boolean func(mobj_t*))
{
    restvisit_t visit;
    P_RestMobj((restmobj_t*)thing, &visit);
    const boolean r = func(&visit.mo);
    P_RestVisited(&visit);
    return r;
}



void P_EffectMobj(const effectmobj_t* effect, mobj_t* mobj)
{
    const statenum_t state = effect->state;
    memset(mobj, 0, sizeof(*mobj));
    mobj->x         = effect->x;
    mobj->y         = effect->y;
    mobj->snext     = effect->snext;
    mobj->sprev     = effect->sprev;
    mobj->subsector = effect->subsector;
    mobj->type      = effect->type & ~FORM_MASK;
    mobj->sprite    = states[state].sprite;
    mobj->z         = effect->z;
    mobj->flags     = mobjinfo[mobj->type].flags;
    mobj->touching_sectorlist = effect->touching_sectorlist;
    mobj->floorz    = effect->floorz;
    mobj->ceilingz  = effect->ceilingz;
    mobj->state     = state;
    mobj->frame     = states[state].frame;
    mobj->tics      = effect->tics;
}

// What P_SpawnMobj does for a puff, blood or teleport fog; P_EndEffect keeps the result.
void P_BeginEffect(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type, effectvisit_t* visit)
{
    if (!(mobjinfo[type].flags & MF_NOBLOCKMAP) || (mobjinfo[type].flags & (MF_NOSECTOR | MF_SOLID | MF_SPECIAL
        | MF_SHOOTABLE | MF_MISSILE | MF_COUNTKILL)))
        I_Error("P_BeginEffect: %i is no effect", type);

    effectmobj_t* effect = Z_MallocLevel(sizeof(effectmobj_t));
    visit->effect = effect;
    memset(&visit->ext, 0, sizeof(visit->ext));
    P_InitMobj(&visit->ext.mo, x, y, z, type, effect);
    effect->thinker.function = TH_EFFECT;
    P_AddThinker(&effect->thinker);
}

// Keeps what spawning or a tic made of an effect, which may only have moved up or down and
// changed its state.
void P_EndEffect(effectvisit_t* visit)
{
    const mobjext_t* ext = &visit->ext;
    const mobj_t* mobj = &ext->mo;
    effectmobj_t* effect = visit->effect;
    const int8_t momz = ext->momz / FRACUNIT;
    if ((fixed_t)momz * FRACUNIT != ext->momz || mobj->tics != (int8_t)mobj->tics || ext->momx || ext->momy
        || mobj->sprite != states[mobj->state].sprite || mobj->frame != states[mobj->state].frame
        || mobj->flags != mobjinfo[mobj->type].flags || mobj->angle)
        I_Error("P_EndEffect: %i does more", mobj->type);

    effect->x         = mobj->x;
    effect->y         = mobj->y;
    effect->snext     = mobj->snext;
    effect->sprev     = mobj->sprev;
    effect->subsector = mobj->subsector;
    effect->type      = mobj->type | FORM_EFFECT;
    effect->tics      = mobj->tics;
    effect->z         = mobj->z;
    effect->touching_sectorlist = mobj->touching_sectorlist;
    effect->floorz    = mobj->floorz;
    effect->ceilingz  = mobj->ceilingz;
    effect->state     = mobj->state;
    effect->momz      = momz;
}

mobj_t* P_SpawnFog(fixed_t x, fixed_t y, fixed_t z)
{
    effectvisit_t visit;
    P_BeginEffect(x, y, z, MT_TFOG, &visit);
    P_EndEffect(&visit);
    return (mobj_t*)visit.effect;
}

static effectmobj_t* P_EffectOfThinker(thinker_t* th)
{
    return (effectmobj_t*)((uint8_t*)th - offsetof(effectmobj_t, thinker));
}

// What P_MobjThinker does for an effect.
void P_EffectThinker(thinker_t* th)
{
    effectmobj_t* effect = P_EffectOfThinker(th);
    effectvisit_t visit;
    const zref_t links[3] = { effect->snext, effect->sprev, effect->touching_sectorlist };
    memset(&visit.ext, 0, sizeof(visit.ext));
    P_EffectMobj(effect, &visit.ext.mo);
    visit.ext.thinker.function = TH_MOBJ;
    visit.ext.momz = (fixed_t)effect->momz * FRACUNIT;
    visit.effect = effect;

    P_MobjThinker(&visit.ext.thinker);

    const boolean removed = visit.ext.thinker.function == TH_REMOVETHING;
    if (links[0] != effect->snext || links[1] != effect->sprev || links[2] != effect->touching_sectorlist
        || (!removed && visit.ext.thinker.function != TH_MOBJ) || visit.ext.mo.x != effect->x
        || visit.ext.mo.y != effect->y || visit.ext.mo.floorz != effect->floorz || visit.ext.mo.ceilingz != effect->ceilingz)
        I_Error("P_EffectThinker: %i moved", visit.ext.mo.type);

    if (removed)
    {
        S_StopSound(effect);
        th->function = TH_REMOVEEFFECT;
        return;
    }
    P_EndEffect(&visit);
}

void P_RemoveEffectDelayed(thinker_t* th)
{
    P_UnlinkThinker(th);
    Z_Free(P_EffectOfThinker(th));
}


static uint8_t P_ThinkerFunctionForType(mobjtype_t type, mobj_t* mobj)
{
    //Full thinking ability.
    if(type < MT_MISC0)
        return TH_MOBJ;

    //Just state cycles.
    if(mobj->tics != -1)
        return TH_BRAINLESS;

    //No thinking at all.
    return TH_NONE;
}

//
// P_SpawnMobj
//

mobj_t* P_NewMobj(void)
{
    mobjext_t* ext;

    for(int16_t i = _g_thingPoolSize-1; i >= 0; i--)
    {
        if(_g_thingPool[i].mo.type == MT_NOTHING)
        {
            ext = &_g_thingPool[i];
            memset (ext, 0, sizeof (*ext));

            ext->mo.flags = MF_POOLED;
            return &ext->mo;
        }
    }

    ext = Z_MallocLevel(sizeof(*ext));
    memset (ext, 0, sizeof (*ext));
    return &ext->mo;
}

// What P_SpawnMobj does to the mobj P_NewMobj gave it, with the lists leading to at.
static void P_InitMobj(mobj_t* mobj, fixed_t x, fixed_t y, fixed_t z, mobjtype_t type, void* at)
{
    const state_t*    st;
    const mobjinfo_t* info;

    mobjext_t*  ext  = NULL;

    info = &mobjinfo[type];
    mobj->type = type;
    mobj->x = x;
    mobj->y = y;
    mobj->flags |= info->flags;

    if (mobj->flags & MF_STATIC)
    {
        if (mobj->flags & (MF_SHOOTABLE | MF_MISSILE | MF_COUNTKILL))
            I_Error("P_SpawnMobj: static thing %i acts", type);
    }
    else
    {
        ext = P_Ext(mobj);
        ext->health = info->spawnhealth;

        if (_g_gameskill != sk_nightmare)
            ext->reactiontime = info->reactiontime;
    }

    if (P_Random() % 4 == 1 && (info->flags & MF_COUNTKILL))
        mobj->flags |= MF_LOOKLATE;
    // do not set the state with P_SetMobjState,
    // because action routines can not be called yet

    st = &states[info->spawnstate];

    mobj->state  = info->spawnstate;
    mobj->tics   = st->tics;
    mobj->sprite = st->sprite;
    mobj->frame  = st->frame;
    mobj->touching_sectorlist = 0; // NULL head of sector list // phares 3/13/98

    // set subsector and/or block links

    P_SetThingPositionAs (mobj, at);

    mobj->floorz   = P_ToQuarters(SEC_FLOORHEIGHT(P_SubsectorSector(mobj->subsector)));
    mobj->ceilingz = P_ToQuarters(SEC_CEILINGHEIGHT(P_SubsectorSector(mobj->subsector)));

    mobj->z = z == ONFLOORZ ? P_FromQuarters(mobj->floorz) : z == ONCEILINGZ ?
                                  P_FromQuarters(mobj->ceilingz) - P_Height(mobj) : z;

    if (ext)
    {
        ext->thinker.function = P_ThinkerFunctionForType(type, mobj);
        ext->target = 0;
        if (at == mobj)
            P_AddThinker (&ext->thinker);
    }
    if (!((mobj->flags ^ MF_COUNTKILL) & MF_COUNTKILL))
        _g_totallive++;
}

mobj_t* P_SpawnMobj(fixed_t x,fixed_t y,fixed_t z,mobjtype_t type)
{
    mobj_t* mobj = P_NewMobj();
    P_InitMobj(mobj, x, y, z, type, mobj);
    return mobj;
}

//
// P_RemoveMobj
//

void P_RemoveMobj(mobj_t* mobj)
{
  P_KillLoadNodes(mobj);
  P_UnsetThingPosition (mobj);

  // Delete all nodes on the current sector_list               phares 3/16/98
  P_DelSeclist();

  // stop any playing sound

  S_StopSound (mobj);

  // killough 11/98:
  //
  // Remove any references to other mobjs.
  //
  // Older demos might depend on the fields being left alone, however,
  // if multiple thinkers reference each other indirectly before the
  // end of the current tic.
  // CPhipps - only leave dead references in old demos; I hope lxdoom_1 level
  // demos are rare and don't rely on this. I hope.

  if (!_g_demoplayback && !(mobj->flags & MF_STATIC))
  {
    mobjext_t* ext = P_Ext(mobj);
    ext->target    = 0;
  }
  // free block

  P_RemoveThing (mobj);
}


/*
 * P_FindDoomedNum
 *
 * Finds a mobj type with a matching doomednum
 *
 */

static PUREFUNC int16_t P_FindDoomedNum(int16_t type)
{
    // find which type to spawn
    for (int16_t i = 0; i < NUMMOBJTYPES; i++)
    {
        if (type == mobjinfo[i].doomednum)
            return i;
    }

    I_Error("P_FindDoomedNum: unknown thing %i", type);
}


//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
//  between levels.
//

static void P_SpawnPlayer(int16_t playerx, int16_t playery, int8_t playerangle)
{
  player_t* p;
  mobj_t*   mobj;

  p = &_g_player;

  if (p->playerstate == PST_REBORN)
    G_PlayerReborn ();

  fixed_t x = ((int32_t)playerx) << FRACBITS;
  fixed_t y = ((int32_t)playery) << FRACBITS;
  fixed_t z = ONFLOORZ;
  mobj = P_SpawnMobj (x,y,z, MT_PLAYER);

  // set color translations for player sprites

  mobj->angle      = ANG45 * playerangle;
  P_Ext(mobj)->health     = p->health;

  p->mo            = mobj;
  p->playerstate   = PST_LIVE;
  p->refire        = 0;
  p->message       = NULL;
  p->damagecount   = 0;
  p->bonuscount    = 0;
  p->extralight    = 0;
  p->fixedcolormap = 0;
  p->viewheight    = VIEWHEIGHT;

  // setup gun psprite

  P_SetupPsprites (p);
}


//
// P_SpawnMapThing
// The fields of the mapthing should
// already be in host byte order.
//

// These are Thing flags

// Skill flags.
#define MTF_EASY                1
#define MTF_NORMAL              2
#define MTF_HARD                4
// Deaf monsters/do not react to sound.
#define MTF_AMBUSH              8

boolean P_MapThingSpawns(const mapthing_t* mthing)
{
    if (mthing->type == 1)
        return true;

    if (_g_gameskill == sk_baby || _g_gameskill == sk_easy)
        return (mthing->options & MTF_EASY) != 0;
    else if (_g_gameskill == sk_hard || _g_gameskill == sk_nightmare)
        return (mthing->options & MTF_HARD) != 0;
    else
        return (mthing->options & MTF_NORMAL) != 0;
}


boolean P_MapThingIsStatic(const mapthing_t* mthing)
{
    return mthing->type != 1 && P_FindDoomedNum(mthing->type) >= MT_MISC0;
}


void P_SpawnMapThing(const mapthing_t* mthing)
{
    int16_t     i;
    mobj_t* mobj;
    fixed_t x;
    fixed_t y;

    if (!P_MapThingSpawns(mthing))
        return;

    //Only care about start spot for player 1.
    if (mthing->type == 1)
    {
        P_SpawnPlayer(mthing->x, mthing->y, mthing->angle);
        return;
    }

    // find which type to spawn

    i = P_FindDoomedNum(mthing->type);

    x = ((int32_t)mthing->x) << FRACBITS;
    y = ((int32_t)mthing->y) << FRACBITS;

    if (i >= MT_MISC0 && P_SpawnLoadStatic(x, y, i))
        return;

    mobj = P_SpawnMobj(x, y, ONFLOORZ, i);

    if (mobj->tics > 0)
        mobj->tics = 1 + (P_Random () % mobj->tics);

    if (!((mobj->flags ^ MF_COUNTKILL) & MF_COUNTKILL))
        _g_totalkills++;

    if (mobj->flags & MF_COUNTITEM)
        _g_totalitems++;

    mobj->angle = ANG45 * mthing->angle;
    if (mthing->options & MTF_AMBUSH)
        mobj->flags |= MF_AMBUSH;

    P_SleepAtLoad(mobj);
}


//
// GAME SPAWN FUNCTIONS
//

//
// P_SpawnPuff
//
void P_SpawnPuff(fixed_t x,fixed_t y,fixed_t z)
  {
  effectvisit_t visit;
  mobj_t* th = &visit.ext.mo;
  // killough 5/5/98: remove dependence on order of evaluation:
  int32_t t = P_Random();
  z += (t - P_Random())<<10;

  P_BeginEffect(x, y, z, MT_PUFF, &visit);
  visit.ext.momz = FRACUNIT;
  th->tics -= P_Random()&3;

  if (th->tics < 1)
    th->tics = 1;

  // don't make punches spark on the wall

  if (P_IsAttackRangeMeleeRange())
    P_SetMobjState (th, S_PUFF3);
  P_EndEffect(&visit);
  }


//
// P_SpawnBlood
//
void P_SpawnBlood(fixed_t x,fixed_t y,fixed_t z,int16_t damage)
  {
  effectvisit_t visit;
  mobj_t* th = &visit.ext.mo;
  // killough 5/5/98: remove dependence on order of evaluation:
  fixed_t t = P_Random();
  z += (t - P_Random())<<10;
  P_BeginEffect(x, y, z, MT_BLOOD, &visit);
  visit.ext.momz = FRACUNIT*2;
  th->tics -= P_Random()&3;

  if (th->tics < 1)
    th->tics = 1;

  if (9 <= damage && damage <= 12)
    P_SetMobjState (th,S_BLOOD2);
  else if (damage < 9)
    P_SetMobjState (th,S_BLOOD3);
  P_EndEffect(&visit);
  }


//
// P_CheckMissileSpawn
// Moves the missile forward a bit
//  and possibly explodes it right there.
//

static void P_CheckMissileSpawn(mobj_t* th)
  {
  th->tics -= P_Random()&3;
  if (th->tics < 1)
    th->tics = 1;

  // move a little forward so an angle can
  // be computed if it immediately explodes

  th->x += (P_Ext(th)->momx>>1);
  th->y += (P_Ext(th)->momy>>1);
  th->z += (P_Ext(th)->momz>>1);

  // killough 8/12/98: for non-missile objects (e.g. grenades)
  if (!(th->flags & MF_MISSILE))
    return;

  if (!P_TryMove (th, th->x, th->y))
    P_ExplodeMissile (th);
  }


//
// P_SpawnMissile
//

mobj_t* P_SpawnMissile(mobj_t* source, mobj_t* dest, mobjtype_t type)
  {
  mobj_t* th;
  angle_t an;
  fixed_t     dist;

  th = P_SpawnMobj (source->x,source->y,source->z + 4*8*FRACUNIT,type);

  if (mobjinfo[th->type].seesound)
    S_StartSound (th, mobjinfo[th->type].seesound);

  P_Ext(th)->target = Z_Ref(source);    // where it came from
  an = R_PointToAngle2 (source->x, source->y, dest->x, dest->y);

  // fuzzy player

  if (dest->flags & MF_SHADOW)
    {  // killough 5/5/98: remove dependence on order of evaluation:
    int32_t t = P_Random();
    an += (t - P_Random())<<20;
    }

  // Nightmare's imp and baron balls are fast.
  fixed_t speed = mobjinfo[th->type].speed;
  if (_g_gameskill == sk_nightmare && (type == MT_TROOPSHOT || type == MT_BRUISERSHOT))
    speed = 20*FRACUNIT;

  th->angle = an;
  an >>= ANGLETOFINESHIFT;
  P_Ext(th)->momx = FixedMulAngle(speed, finecosine(an));
  P_Ext(th)->momy = FixedMulAngle(speed, finesine(  an));

  dist = P_AproxDistance (dest->x - source->x, dest->y - source->y);
  dist = dist / speed;

  if (dist < 1)
    dist = 1;

  P_Ext(th)->momz = (dest->z - source->z) / dist;
  P_CheckMissileSpawn (th);

  return th;
  }


//
// P_SpawnPlayerMissile
// Tries to aim at a nearby monster
//

void P_SpawnPlayerMissile(mobj_t* source)
{
	mobj_t* th;
	fixed_t x, y, z, slope = 0;

	// see which target is to be aimed at

	angle_t an = source->angle;

	slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
	if (!_g_linetarget)
		slope = P_AimLineAttack(source, an += 1 << 26, 16 * 64 * FRACUNIT);
	if (!_g_linetarget)
		slope = P_AimLineAttack(source, an -= 2l << 26, 16 * 64 * FRACUNIT);
	if (!_g_linetarget)
		an = source->angle, slope = 0;

	x = source->x;
	y = source->y;
	z = source->z + 4 * 8 * FRACUNIT;

	th = P_SpawnMobj(x,y,z, MT_ROCKET);

	if (mobjinfo[th->type].seesound)
		S_StartSound(th, mobjinfo[th->type].seesound);

	P_Ext(th)->target = Z_Ref(source);
	th->angle  = an;
	P_Ext(th)->momx   = FixedMulAngle(mobjinfo[th->type].speed,finecosine(an >> ANGLETOFINESHIFT));
	P_Ext(th)->momy   = FixedMulAngle(mobjinfo[th->type].speed,finesine(  an >> ANGLETOFINESHIFT));
	P_Ext(th)->momz   = FixedMul(mobjinfo[th->type].speed,slope);

	P_CheckMissileSpawn(th);
}


struct player_s* P_MobjIsPlayer(const mobj_t* mobj)
{
    if(mobj == _g_player.mo)
    {
        return &_g_player;
    }

    return NULL;
}
