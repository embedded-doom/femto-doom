// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2023-2026 by Frenkel Smeijers
// Copyright (C) 2026 by Jakob Jungreuthmayer
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// $Log:$
//
// DESCRIPTION:
//	Zone Memory Allocation. Neat.
//
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "z_zone.h"
#include "doomdef.h"
#include "i_platform.h"
#include "options.h"


//
// ZONE MEMORY
// Tags, 0 for a free block.
#define PU_STATIC		1	// static entire execution time
#define PU_LEVEL		2	// static until level exited
#define PU_LEVSPEC		3	// a special thinker in a level


//
// ZONE MEMORY ALLOCATION
//
// There is never any space between memblocks,
//  and there will never be two contiguous free memblocks.
// A block comes from the smallest free block that fits, which leaves the larger ones whole.
//

#if defined INSTRUMENTED
    static int32_t running_count = 0;
#endif


#define	ZONEID	0x1dea

// Blocks are addressed by index, in units from _g_zonebase that keep pointers aligned.
#define ZONE_UNIT	(sizeof(void*) > 4 ? 8 : 4)
typedef uint16_t zindex_t;

// The next block starts where this one ends, the last is followed by the sentinal.
typedef struct
{
    uint32_t  size : 15;		// in ZONE_UNIT, including the header and possibly tiny fragments
    uint32_t  prev : 15;
    uint32_t  tag  : 2;			// PU_*, 0 for a free block
#if defined ZONEIDCHECK
    uint16_t id;			// should be ZONEID
#endif
} memblock_t;

#define HEADER_UNITS	((sizeof(memblock_t) + ZONE_UNIT - 1) / ZONE_UNIT)

typedef char assertZoneFitsBlockSize[ZREF_SPAN / ZONE_UNIT < (1 << 15) ? 1 : -1];
#if !defined ZONEIDCHECK
typedef char assertBlockHeaderSize[sizeof(memblock_t) == 4 ? 1 : -1];
#endif


uint8_t* _g_zonebase;

static memblock_t* mainzone_sentinal;
static zindex_t   mainzone_end;


static zindex_t pointerToIndex(const void* ptr)
{
	return ((const uint8_t*)ptr - _g_zonebase) / ZONE_UNIT;
}

static memblock_t* indexToPointer(zindex_t i)
{
	return (memblock_t*)(_g_zonebase + (uint32_t)i * ZONE_UNIT);
}

static zindex_t nextIndex(const memblock_t* block)
{
	const zindex_t next = pointerToIndex(block) + block->size;
	return next == mainzone_end ? pointerToIndex(mainzone_sentinal) : next;
}


boolean Z_EqualNames(const char* farName, const char* nearName)
{
	return memcmp(farName, nearName, 8) == 0;
}

void Z_CopyName(char* name8, const char* name)
{
	int16_t i = 0;
	for (; i < 8 && name[i]; i++)
		name8[i] = name[i];
	for (; i < 8; i++)
		name8[i] = 0;
}


zref_t Z_Ref(const void* ptr)
{
	if (ptr == NULL)
		return 0;

	uint32_t offset = (const uint8_t*)ptr - _g_zonebase;
	if (offset == 0 || offset > ZREF_SPAN || (offset & ((1 << ZREF_SHIFT) - 1)) != 0)
		I_Error("Z_Ref: 0x%lx is not a zone address", (unsigned long)offset);

	return offset >> ZREF_SHIFT;
}


//
// Z_Init
//
void Z_Init (void)
{
	// allocate all available conventional memory.
	uint32_t heapSize;
	static uint8_t* mainzone; mainzone = I_ZoneBase(&heapSize);

	_g_zonebase = mainzone;
	if (heapSize > ZREF_SPAN)
		heapSize = ZREF_SPAN;
	heapSize &= ~(uint32_t)(ZONE_UNIT - 1);

	// the sentinal heads the zone, so no object lives at offset 0
	mainzone_sentinal = (memblock_t*)mainzone;
	zindex_t sentinal = pointerToIndex(mainzone_sentinal);

	// set the rest of the zone to one free block
	const zindex_t first = sentinal + HEADER_UNITS;
	memblock_t* block = indexToPointer(first);

	mainzone_sentinal->size = HEADER_UNITS;
	mainzone_sentinal->tag  = PU_STATIC;
	mainzone_sentinal->prev = first;

	block->size = heapSize / ZONE_UNIT - HEADER_UNITS;
	mainzone_end = heapSize / ZONE_UNIT;
	block->tag  = 0;
	block->prev = sentinal;
#if defined ZONEIDCHECK
	block->id   = ZONEID;
#endif

	I_Printf("%lu bytes allocated for zone\n", (unsigned long)block->size * ZONE_UNIT);
}


static void Z_FreeBlock(memblock_t* block)
{
#if defined ZONEIDCHECK
    if (block->id != ZONEID)
        I_Error("Z_FreeBlock: block has id %x instead of ZONEID", block->id);
#endif

    // mark as free
    block->tag = 0;


#if defined INSTRUMENTED
    running_count -= block->size * ZONE_UNIT;
    I_Printf("Free: %ld\n", (long)running_count);
#endif

    memblock_t* other = indexToPointer(block->prev);

    if (!other->tag)
    {
        // merge with previous free block
        other->size += block->size;
        indexToPointer(nextIndex(other))->prev = block->prev; // == pointerToIndex(other);

        block = other;
    }

    other = indexToPointer(nextIndex(block));
    if (!other->tag)
    {
        // merge the next free block onto the end
        block->size += other->size;
        indexToPointer(nextIndex(block))->prev = pointerToIndex(block);
    }
}


//
// Z_Free
//
void Z_Free (const void* ptr)
{
	Z_FreeBlock(indexToPointer(pointerToIndex(ptr) - HEADER_UNITS));
}


static uint32_t Z_GetLargestFreeBlockSize(void)
{
	zindex_t sentinal = pointerToIndex(mainzone_sentinal);

	uint32_t largestFreeBlockSize = 0;

	for (memblock_t* block = indexToPointer(nextIndex(mainzone_sentinal)); pointerToIndex(block) != sentinal; block = indexToPointer(nextIndex(block)))
		if (!block->tag && (uint32_t)block->size * ZONE_UNIT > largestFreeBlockSize)
			largestFreeBlockSize = (uint32_t)block->size * ZONE_UNIT;

	return largestFreeBlockSize;
}

static uint32_t Z_GetTotalFreeMemory(void)
{
	zindex_t sentinal = pointerToIndex(mainzone_sentinal);

	uint32_t totalFreeMemory = 0;

	for (memblock_t* block = indexToPointer(nextIndex(mainzone_sentinal)); pointerToIndex(block) != sentinal; block = indexToPointer(nextIndex(block)))
		if (!block->tag)
			totalFreeMemory += (uint32_t)block->size * ZONE_UNIT;

	return totalFreeMemory;
}


//
// Z_TryMalloc
//
static void* Z_TryMalloc(uint16_t size, int8_t tag)
{
    // account for size of block header
    const uint16_t units = (size + (ZONE_UNIT - 1)) / ZONE_UNIT + HEADER_UNITS;

    // the smallest free block that fits, of equal ones the first
    zindex_t sentinal = pointerToIndex(mainzone_sentinal);
    memblock_t* base = NULL;
    for (memblock_t* block = indexToPointer(nextIndex(mainzone_sentinal)); pointerToIndex(block) != sentinal; block = indexToPointer(nextIndex(block)))
        if (!block->tag && block->size >= units && (!base || block->size < base->size))
        {
            base = block;
            if (block->size == units)
                break;
        }

    if (!base)
        return NULL;

    uint16_t newblock_units = base->size - units;
    if ((uint32_t)newblock_units * ZONE_UNIT > MIN_FRAGMENT)
    {
        // there will be a free fragment after the allocated block
        zindex_t base_index     = pointerToIndex(base);
        zindex_t newblock_index = base_index + units;

        memblock_t* newblock = indexToPointer(newblock_index);
        newblock->size = newblock_units;
        newblock->tag  = 0;
        newblock->prev = base_index;
#if defined ZONEIDCHECK
        newblock->id   = ZONEID;
#endif

        indexToPointer(nextIndex(base))->prev = newblock_index;
        base->size = units;
    }

    base->tag = tag;
#if defined ZONEIDCHECK
    base->id  = ZONEID;
#endif

#if defined INSTRUMENTED
    running_count += base->size * ZONE_UNIT;
    I_Printf("Alloc: %ld (%ld)\n", (long)(base->size * ZONE_UNIT), (long)running_count);
#endif

    return indexToPointer(pointerToIndex(base) + HEADER_UNITS);
}


static void* Z_Malloc(uint16_t size, int8_t tag) {
	void* ptr = Z_TryMalloc(size, tag);
	if (!ptr)
		I_Error ("Z_Malloc: failed to allocate %u B, max free block %lu B, total free %lu", size, (unsigned long)Z_GetLargestFreeBlockSize(), (unsigned long)Z_GetTotalFreeMemory());
	return ptr;
}


void* Z_MallocStatic(uint16_t size)
{
	return Z_Malloc(size, PU_STATIC);
}


void* Z_MallocLevel(uint16_t size)
{
	return Z_Malloc(size, PU_LEVEL);
}


void* Z_CallocLevel(uint16_t size)
{
    void* ptr = Z_Malloc(size, PU_LEVEL);
    memset(ptr, 0, size);
    return ptr;
}


void* Z_CallocLevSpec(uint16_t size)
{
	void* ptr = Z_Malloc(size, PU_LEVSPEC);
	memset(ptr, 0, size);
	return ptr;
}


//
// Z_FreeTags
//
void Z_FreeTags(void)
{
    zindex_t sentinal = pointerToIndex(mainzone_sentinal);
    memblock_t* next;

    for (memblock_t* block = indexToPointer(nextIndex(mainzone_sentinal)); pointerToIndex(block) != sentinal; block = next)
    {
        // get link before freeing
        next = indexToPointer(nextIndex(block));

        if (block->tag == PU_LEVEL || block->tag == PU_LEVSPEC)
            Z_FreeBlock(block);
    }
}

//
// Z_CheckHeap
//
void Z_CheckHeap (void)
{
    zindex_t sentinal = pointerToIndex(mainzone_sentinal);

    for (memblock_t* block = indexToPointer(nextIndex(mainzone_sentinal)); ; block = indexToPointer(nextIndex(block)))
    {
        if (nextIndex(block) == sentinal)
        {
            // all blocks have been hit
            break;
        }

#if defined ZONEIDCHECK
        if (block->id != ZONEID)
            I_Error("Z_CheckHeap: block has id %x instead of ZONEID", block->id);
#endif

        if (indexToPointer(nextIndex(block))->prev != pointerToIndex(block))
            I_Error ("Z_CheckHeap: next block doesn't have proper back link\n");

        if (!block->tag && !indexToPointer(nextIndex(block))->tag)
            I_Error ("Z_CheckHeap: two consecutive free blocks\n");
    }
}
