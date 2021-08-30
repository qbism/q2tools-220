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

//
// l3dslib.c: library for loading triangles from an Alias triangle file
//

#include <stdio.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "trilib.h"
#include "l3dslib.h"

#define MAIN3DS       0x4D4D
#define EDIT3DS       0x3D3D  // this is the start of the editor config
#define EDIT_OBJECT   0x4000
#define OBJ_TRIMESH   0x4100
#define TRI_VERTEXL   0x4110
#define TRI_FACEL1    0x4120

#define MAXVERTS	2000

typedef struct {
	int32_t	v[4];
} tri;

float	fverts[MAXVERTS][3];
tri		tris[MAXTRIANGLES];

int32_t	bytesread, level, numtris, totaltris;
int32_t	vertsfound, trisfound;

triangle_t	*ptri;

// Alias stores triangles as 3 explicit vertices in .tri files, so even though we
// start out with a vertex pool and vertex indices for triangles, we have to convert
// to raw, explicit triangles
void StoreAliasTriangles (void)
{
	int32_t		i, j, k;

	if ((totaltris + numtris) > MAXTRIANGLES)
		Error ("Error: Too many triangles");

	for (i=0; i<numtris ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			for (k=0 ; k<3 ; k++)
			{
				ptri[i+totaltris].verts[j][k] = fverts[tris[i].v[j]][k];
			}
		}
	}

	totaltris += numtris;
	numtris = 0;
	vertsfound = 0;
	trisfound = 0;
}


int32_t ParseVertexL (FILE *input)
{
	int32_t				i, j, startbytesread, numverts;
	uint16_t	tshort;

	if (vertsfound)
		Error ("Error: Multiple vertex chunks");

	vertsfound = 1;
	startbytesread = bytesread;

	if (feof(input))
		Error ("Error: unexpected end of file");

	fread(&tshort, sizeof(tshort), 1, input);
	bytesread += sizeof(tshort);
	numverts = (int32_t)tshort;

	if (numverts > MAXVERTS)
		Error ("Error: Too many vertices");

	for (i=0 ; i<numverts ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			if (feof(input))
				Error ("Error: unexpected end of file");

			fread(&fverts[i][j], sizeof(float), 1, input);
			bytesread += sizeof(float);
		}
	}

	if (vertsfound && trisfound)
		StoreAliasTriangles ();

	return bytesread - startbytesread;
}


int32_t ParseFaceL1 (FILE *input)
{

	int32_t				i, j, startbytesread;
	uint16_t	tshort;

	if (trisfound)
		Error ("Error: Multiple face chunks");

	trisfound = 1;
	startbytesread = bytesread;

	if (feof(input))
		Error ("Error: unexpected end of file");

	fread(&tshort, sizeof(tshort), 1, input);
	bytesread += sizeof(tshort);
	numtris = (int32_t)tshort;

	if (numtris > MAXTRIANGLES)
		Error ("Error: Too many triangles");

	for (i=0 ; i<numtris ; i++)
	{
		for (j=0 ; j<4 ; j++)
		{
			if (feof(input))
				Error ("Error: unexpected end of file");

			fread(&tshort, sizeof(tshort), 1, input);
			bytesread += sizeof(tshort);
			tris[i].v[j] = (int32_t)tshort;
		}
	}

	if (vertsfound && trisfound)
		StoreAliasTriangles ();

	return bytesread - startbytesread;
}


int32_t ParseChunk (FILE *input)
{
#define CHUNK_SIZE	4096
	char			temp[CHUNK_SIZE];
	uint16_t	type;
	int32_t				i, length, w, t, retval;

	level++;
	retval = 0;

// chunk type
	if (feof(input))
		Error ("Error: unexpected end of file");

	fread(&type, sizeof(type), 1, input);
	bytesread += sizeof(type);

// chunk length
	if (feof(input))
		Error ("Error: unexpected end of file");

	fread (&length, sizeof(length), 1, input);
	bytesread += sizeof(length);
	w = length - 6;

// process chunk if we care about it, otherwise skip it
	switch (type)
	{
	case TRI_VERTEXL:
		w -= ParseVertexL (input);
		goto ParseSubchunk;

	case TRI_FACEL1:
		w -= ParseFaceL1 (input);
		goto ParseSubchunk;

	case EDIT_OBJECT:
	// read the name
		i = 0;

		do
		{
			if (feof(input))
				Error ("Error: unexpected end of file");

			fread (&temp[i], 1, 1, input);
			i++;
			w--;
			bytesread++;
		} while (temp[i-1]);

	case MAIN3DS:
	case OBJ_TRIMESH:
	case EDIT3DS:
	// parse through subchunks
ParseSubchunk:
		while (w > 0)
		{
			w -= ParseChunk (input);
		}

		retval = length;
		goto Done;

	default:
	// skip other chunks
		while (w > 0)
		{
			t = w;

			if (t > CHUNK_SIZE)
				t = CHUNK_SIZE;

			if (feof(input))
				Error ("Error: unexpected end of file");

			fread (&temp, t, 1, input);
			bytesread += t;

			w -= t;
		}

		retval = length;
		goto Done;
	}

Done:
	level--;
	return retval;
}


void Load3DSTriangleList (char *filename, triangle_t **pptri, int32_t *numtriangles)
{
	FILE        *input;
	int16_t	tshort;

	bytesread = 0;
	level = 0;
	numtris = 0;
	totaltris = 0;
	vertsfound = 0;
	trisfound = 0;

	if ((input = fopen(filename, "rb")) == 0) {
		fprintf(stderr,"reader: could not open file '%s'\n", filename);
		exit(0);
	}

	fread(&tshort, sizeof(tshort), 1, input);

// should only be MAIN3DS, but some files seem to start with EDIT3DS, with
// no MAIN3DS
	if ((tshort != MAIN3DS) && (tshort != EDIT3DS)) {
		fprintf(stderr,"File is not a 3DS file.\n");
		exit(0);
	}

// back to top of file so we can parse the first chunk descriptor
	fseek(input, 0, SEEK_SET);

	ptri = malloc (MAXTRIANGLES * sizeof(triangle_t));

	*pptri = ptri;

// parse through looking for the relevant chunk tree (MAIN3DS | EDIT3DS | EDIT_OBJECT |
// OBJ_TRIMESH | {TRI_VERTEXL, TRI_FACEL1}) and skipping other chunks
	ParseChunk (input);

	if (vertsfound || trisfound)
		Error ("Incomplete triangle set");

	*numtriangles = totaltris;

	fclose (input);
	free (ptri); //qb: stop mem leak
}

