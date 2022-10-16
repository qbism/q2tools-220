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

// piclib.h

void LoadLBM(char *filename, byte **picture, byte **palette);
void WriteLBMfile(char *filename, byte *data, int32_t width, int32_t height, byte *palette);
void LoadPCX(char *filename, byte **picture, byte **palette, int32_t *width, int32_t *height);
void WritePCXfile(char *filename, byte *data, int32_t width, int32_t height, byte *palette);

// loads / saves either lbm or pcx, depending on extension
void Load256Image(char *name, byte **pixels, byte **palette,
                  int32_t *width, int32_t *height);
void Save256Image(char *name, byte *pixels, byte *palette,
                  int32_t width, int32_t height);

void LoadTGA(char *filename, byte **pixels, int32_t *width, int32_t *height);
