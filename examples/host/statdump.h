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
 *      The statistics of completed levels, as statdump writes them.
 *
 *-----------------------------------------------------------------------------*/

#ifndef STATDUMP_H
#define STATDUMP_H

// Remembers the statistics of a level that has just been completed.
void Stat_Capture(void);

// Writes the remembered statistics.
void Stat_Dump(const char* path);

#endif
