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

#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"

//#define	MAX_PORTALS	32767

#define MAX_PORTALS_QBSP MAX_MAP_PORTALS_QBSP / 2 // qb: half

#define PORTALFILE       "PRT1"

#define ON_EPSILON       0.1

typedef struct
{
    vec3_t normal;
    float dist;
} plane_t;

typedef struct
{
    qboolean original; // don't free, it's part of the portal
    int32_t numpoints;
    vec3_t points[MAX_POINTS_ON_FIXED_WINDING]; // variable sized
} winding_t;

winding_t *NewWinding(int32_t points);
void FreeWinding(winding_t *w);
winding_t *CopyWinding(winding_t *w);

typedef enum { stat_none,
               stat_working,
               stat_done } vstatus_t;
typedef struct
{
    plane_t plane; // normal pointing into neighbor
    int32_t leaf;  // neighbor

    vec3_t origin; // for fast clip testing
    float radius;

    winding_t *winding;
    vstatus_t status;
    byte *portalfront; // [portals], preliminary
    byte *portalflood; // [portals], intermediate
    byte *portalvis;   // [portals], final

    int32_t nummightsee; // bit count on portalflood for sort
} portal_t;

typedef struct seperating_plane_s {
    struct seperating_plane_s *next;
    plane_t plane; // from portal is on positive side
} sep_t;

typedef struct passage_s {
    struct passage_s *next;
    int32_t from, to; // leaf numbers
    sep_t *planes;
} passage_t;

#define MAX_PORTALS_ON_LEAF 1024 //qb: was 128
typedef struct leaf_s {
    int32_t numportals;
    passage_t *passages;
    portal_t *portals[MAX_PORTALS_ON_LEAF];
} leaf_t;

typedef struct pstack_s {
    byte mightsee[MAX_PORTALS_QBSP / 8]; // bit string
    struct pstack_s *next;
    leaf_t *leaf;
    portal_t *portal; // portal exiting
    winding_t *source;
    winding_t *pass;

    winding_t windings[3]; // source, pass, temp in any order
    int32_t freewindings[3];

    plane_t portalplane;
} pstack_t;

typedef struct
{
    portal_t *base;
    int32_t c_chains;
    pstack_t pstack_head;
} threaddata_t;

extern int32_t numportals;
extern int32_t portalclusters;

extern portal_t *portals;
extern leaf_t *leafs;

extern int32_t c_portaltest, c_portalpass, c_portalcheck;
extern int32_t c_portalskip, c_leafskip;
extern int32_t c_vistest, c_mighttest;
extern int32_t c_chains;

extern byte *vismap, *vismap_p, *vismap_end; // past visfile

extern byte *uncompressed;

extern int32_t leafbytes, leaflongs;
extern int32_t portalbytes, portallongs;

void LeafFlow(int32_t leafnum);

void BasePortalVis(int32_t portalnum);
void BetterPortalVis(int32_t portalnum);
void PortalFlow(int32_t portalnum);

extern portal_t *sorted_portals[MAX_MAP_PORTALS_QBSP * 2];

int32_t CountBits(byte *bits, int32_t numbits);
