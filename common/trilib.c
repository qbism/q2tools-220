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
// trilib.c: library for loading triangles from an Alias triangle file
//

#include <stdio.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "4data.h"

// on disk representation of a face

#define FLOAT_START BOGUS_RANGE
#define FLOAT_END   -FLOAT_START
#define MAGIC       123322

//#define NOISY 1

typedef struct {
    float v[3];
} vector;

typedef struct
{
    vector n; /* normal */
    vector p; /* point */
    vector c; /* color */
    float u;  /* u */
    float v;  /* v */
} aliaspoint_t;

typedef struct {
    aliaspoint_t pt[3];
} tf_triangle;

void ByteSwapTri(tf_triangle *tri) {
    int32_t i;

    for (i = 0; i < sizeof(tf_triangle) / 4; i++) {
        ((int32_t *)tri)[i] = BigLong(((int32_t *)tri)[i]);
    }
}

void LoadTriangleList(char *filename, triangle_t **pptri, int32_t *numtriangles) {
    FILE *input;
    float start;
    char name[256], tex[256];
    int32_t i, count, magic;
    tf_triangle tri;
    triangle_t *ptri;
    int32_t iLevel;
    int32_t exitpattern;
    float t;

    t                              = -FLOAT_START;
    *((uint8_t *)&exitpattern + 0) = *((uint8_t *)&t + 3);
    *((uint8_t *)&exitpattern + 1) = *((uint8_t *)&t + 2);
    *((uint8_t *)&exitpattern + 2) = *((uint8_t *)&t + 1);
    *((uint8_t *)&exitpattern + 3) = *((uint8_t *)&t + 0);

    if ((input = fopen(filename, "rb")) == 0)
        Error("reader: could not open file '%s'", filename);

    iLevel = 0;

    fread(&magic, sizeof(int32_t), 1, input);
    if (BigLong(magic) != MAGIC)
        Error("%s is not a Alias object separated triangle file, magic number is wrong.", filename);

    ptri   = malloc(MAXTRIANGLES * sizeof(triangle_t));

    *pptri = ptri;

    while (feof(input) == 0) {
        if (fread(&start, sizeof(float), 1, input) < 1)
            break;
        *(int32_t *)&start = BigLong(*(int32_t *)&start);
        if (*(int32_t *)&start != exitpattern) {
            if (start == FLOAT_START) {
                /* Start of an object or group of objects. */
                i = -1;
                do {
                    /* There are probably better ways to read a string from */
                    /* a file, but this does allow you to do error checking */
                    /* (which I'm not doing) on a per character basis.      */
                    ++i;
                    fread(&(name[i]), sizeof(char), 1, input);
                } while (name[i] != '\0');

                //				indent();
                //				fprintf(stdout,"OBJECT START: %s\n",name);
                fread(&count, sizeof(int32_t), 1, input);
                count = BigLong(count);
                ++iLevel;
                if (count != 0) {
                    //					indent();
                    //					fprintf(stdout,"NUMBER OF TRIANGLES: %d\n",count);

                    i = -1;
                    do {
                        ++i;
                        fread(&(tex[i]), sizeof(char), 1, input);
                    } while (tex[i] != '\0');

                    //					indent();
                    //					fprintf(stdout,"  Object texture name: '%s'\n",tex);
                }

                /* Else (count == 0) this is the start of a group, and */
                /* no texture name is present. */
            } else if (start == FLOAT_END) {
                /* End of an object or group. Yes, the name should be */
                /* obvious from context, but it is in here just to be */
                /* safe and to provide a little extra information for */
                /* those who do not wish to write a recursive reader. */
                /* Mia culpa. */
                --iLevel;
                i = -1;
                do {
                    ++i;
                    fread(&(name[i]), sizeof(char), 1, input);
                } while (name[i] != '\0');

                //				indent();
                //				fprintf(stdout,"OBJECT END: %s\n",name);
                continue;
            }
        }

        //
        // read the triangles
        //
        for (i = 0; i < count; ++i) {
            int32_t j;

            fread(&tri, sizeof(tf_triangle), 1, input);
            ByteSwapTri(&tri);
            for (j = 0; j < 3; j++) {
                int32_t k;

                for (k = 0; k < 3; k++) {
                    ptri->verts[j][k] = tri.pt[j].p.v[k];
                }
            }

            ptri++;

            if ((ptri - *pptri) >= MAXTRIANGLES)
                Error("Error: too many triangles; increase MAXTRIANGLES\n");
        }
    }

    *numtriangles = ptri - *pptri;

    fclose(input);
    free(ptri); // qb: stop mem leak
}
