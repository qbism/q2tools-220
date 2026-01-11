/*
===========================================================================
Copyright (C) 1997-2006 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
===========================================================================
*/

// scriplib.h

#ifndef __CMDLIB__
#include "cmdlib.h"
#endif

#define MAXTOKEN 512

extern char token[MAXTOKEN];
extern char *scriptbuffer, *script_p, *scriptend_p;
extern int32_t grabbed;
extern int32_t scriptline;
extern bool endofscript;

extern char brush_info[2000];
void MarkBrushBegin();

void LoadScriptFile(char *filename);
void ParseFromMemory(char *buffer, int32_t size);

bool GetToken(bool crossline);
void UnGetToken(void);
bool TokenAvailable(void);
