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

// cmdlib.h

#ifndef __CMDLIB__
#define __CMDLIB__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

// the dec offsetof macro doesnt work very well...
#define myoffsetof(type, identifier) ((size_t) & ((type *)0)->identifier)

// set these before calling CheckParm
extern int32_t myargc;
extern char **myargv;

char *strtoupper(char *in);
char *strlower(char *in);
int32_t Q_strncasecmp(char *s1, char *s2, int32_t n);
int32_t Q_strcasecmp(char *s1, char *s2);
void Q_pathslash(char *out); //qb: added
void Q_getwd(char *out);

int32_t Q_filelength(FILE *f);
int32_t FileTime(char *path);

void Q_mkdir(char *path);

extern char qdir[1024];
extern char gamedir[1024];
extern char basedir[1024];
extern char moddir[1024];
void SetQdirFromPath(char *path);
char *ExpandArg(const char *path);  // from cmd line
char *ExpandPath(char *path); // from scripts
char *ExpandPathAndArchive(char *path);

double I_FloatTime(void);

void Error(char *error, ...);
int32_t CheckParm(char *check);

FILE *SafeOpenWrite(char *filename);
FILE *SafeOpenRead(char *filename);
void SafeRead(FILE *f, void *buffer, int32_t count);
void SafeWrite(FILE *f, void *buffer, int32_t count);

int32_t LoadFile(char *filename, void **bufferptr);
int32_t TryLoadFile(char *filename, void **bufferptr, int32_t print_error);
int32_t TryLoadFileFromPak(char *filename, void **bufferptr, char *gamedir);
void SaveFile(char *filename, void *buffer, int32_t count);
bool FileExists(char *filename);

void DefaultExtension(char *path, char *extension);
void DefaultPath(char *path, char *basepath);
void StripFilename(char *path);
void StripExtension(char *path);

void ExtractFilePath(char *path, char *dest);
void ExtractFileBase(char *path, char *dest);
void ExtractFileExtension(char *path, char *dest);

int32_t ParseNum(char *str);

//qb:  change to defines
#define LittleShort(x)    ((uint16_t)(x))
#define LittleLong(x)     ((uint32_t)(x))
#define LittleFloat(x)    ((float)(x))

short BigShort(short l);
int32_t BigLong(int32_t l);
float BigFloat(float l);
char *COM_Parse(char *data);

extern char com_token[1024];
extern bool com_eof;

char *copystring(char *s);

void CRC_Init(uint16_t *crcvalue);
void CRC_ProcessByte(uint16_t *crcvalue, uint8_t data);
uint16_t CRC_Value(uint16_t crcvalue);

void CreatePath(char *path);
void QCopyFile(char *from, char *to);

extern bool archive;
extern char archivedir[1024];

extern bool verbose;
void qprintf(char *format, ...);

void ExpandWildcards(int32_t *argc, char ***argv);

// for compression routines
typedef struct
{
    uint8_t *data;
    int32_t count;
} cblock_t;

#endif
