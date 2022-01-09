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

#include "vis.h"
#include "threads.h"
#include "stdlib.h"

extern qboolean use_qbsp;

int32_t numportals;
int32_t portalclusters;

char inbase[32];
char outbase[32];

portal_t *portals;
leaf_t *leafs;

int32_t c_portaltest, c_portalpass, c_portalcheck;

byte *uncompressedvis;

byte *vismap, *vismap_p, *vismap_end; // past visfile
int32_t originalvismapsize;

int32_t leafbytes; // (portalclusters+63)>>3
int32_t leaflongs;

int32_t portalbytes, portallongs;

qboolean fastvis;
qboolean nosort;

int32_t totalvis;

portal_t *sorted_portals[MAX_MAP_PORTALS_QBSP * 2];

//=============================================================================

void PlaneFromWinding(winding_t *w, plane_t *plane) {
    vec3_t v1, v2;

    // calc plane
    VectorSubtract(w->points[2], w->points[1], v1);
    VectorSubtract(w->points[0], w->points[1], v2);
    CrossProduct(v2, v1, plane->normal);
    VectorNormalize(plane->normal, plane->normal);
    plane->dist = DotProduct(w->points[0], plane->normal);
}

/*
==================
NewWinding
==================
*/
winding_t *NewWinding(int32_t points) {
    winding_t *w;
    int32_t size;

    if (points > MAX_POINTS_ON_WINDING)
        Error("NewWinding: %i points", points);

    size = (intptr_t)((winding_t *)0)->points[points];
    w    = malloc(size);
    memset(w, 0, size);

    return w;
}

void prl(leaf_t *l) {
    int32_t i;
    portal_t *p;
    plane_t pl;

    for (i = 0; i < l->numportals; i++) {
        p  = l->portals[i];
        pl = p->plane;
        printf("portal %4i to leaf %4i : %7.1f : (%4.1f, %4.1f, %4.1f)\n", (int32_t)(p - portals), p->leaf, pl.dist, pl.normal[0], pl.normal[1], pl.normal[2]);
    }
}

//=============================================================================

/*
=============
SortPortals

Sorts the portals from the least complex, so the later ones can reuse
the earlier information.
=============
*/
int32_t PComp(const void *a, const void *b) {
    if ((*(portal_t **)a)->nummightsee == (*(portal_t **)b)->nummightsee)
        return 0;
    if ((*(portal_t **)a)->nummightsee < (*(portal_t **)b)->nummightsee)
        return -1;
    return 1;
}
void SortPortals(void) {
    int32_t i;

    for (i = 0; i < numportals * 2; i++)
        sorted_portals[i] = &portals[i];

    if (nosort)
        return;
    qsort(sorted_portals, numportals * 2, sizeof(sorted_portals[0]), PComp);
}

/*
==============
LeafVectorFromPortalVector
==============
*/
int32_t LeafVectorFromPortalVector(byte *portalbits, byte *leafbits) {
    int32_t i;
    portal_t *p;
    int32_t c_leafs;

    memset(leafbits, 0, leafbytes);

    for (i = 0; i < numportals * 2; i++) {
        if (portalbits[i >> 3] & (1 << (i & 7))) {
            p = portals + i;
            leafbits[p->leaf >> 3] |= (1 << (p->leaf & 7));
        }
    }

    c_leafs = CountBits(leafbits, portalclusters);

    return c_leafs;
}

/*
===============
ClusterMerge

Merges the portal visibility for a leaf
===============
*/
void ClusterMerge(int32_t leafnum) {
    leaf_t *leaf;
    byte portalvector[MAX_PORTALS_QBSP / 8];
    byte uncompressed[MAX_MAP_LEAFS_QBSP / 8];
    byte compressed[MAX_MAP_LEAFS_QBSP / 8];
    int32_t i, j;
    int32_t numvis;
    byte *dest;
    portal_t *p;
    int32_t pnum;

    // OR together all the portalvis bits

    memset(portalvector, 0, portalbytes);
    leaf = &leafs[leafnum];

    for (i = 0; i < leaf->numportals; i++) {
        p = leaf->portals[i];
        if (p->status != stat_done)
            Error("portal not done");
        for (j = 0; j < portallongs; j++)
            ((long *)portalvector)[j] |= ((long *)p->portalvis)[j];
        pnum = p - portals;
        portalvector[pnum >> 3] |= 1 << (pnum & 7);
    }

    // convert portal bits to leaf bits
    numvis = LeafVectorFromPortalVector(portalvector, uncompressed);

    if (uncompressed[leafnum >> 3] & (1 << (leafnum & 7)))
        printf("WARNING: Leaf portals saw into leaf\n");

    uncompressed[leafnum >> 3] |= (1 << (leafnum & 7));
    numvis++; // count the leaf itself

    // save uncompressed for PHS calculation
    memcpy(uncompressedvis + leafnum * leafbytes, uncompressed, leafbytes);

    //
    // compress the bit string
    //
    qprintf("cluster %4i : %4i visible\n", leafnum, numvis);
    totalvis += numvis;

    i    = CompressVis(uncompressed, compressed);

    dest = vismap_p;
    vismap_p += i;

    if (vismap_p > vismap_end)
        Error("Vismap expansion overflow. Exceeds extended limit");

    dvis->bitofs[leafnum][DVIS_PVS] = dest - vismap;
    memcpy(dest, compressed, i);
}

/*
==================
CalcPortalVis
==================
*/
void CalcPortalVis(void) {
    int32_t i;

    // fastvis just uses mightsee for a very loose bound
    if (fastvis) {
        for (i = 0; i < numportals * 2; i++) {
            portals[i].portalvis = portals[i].portalflood;
            portals[i].status    = stat_done;
        }
        return;
    }

    RunThreadsOnIndividual(numportals * 2, true, PortalFlow);
}

/*
==================
CalcVis
==================
*/
void CalcVis(void) {
    int32_t i;

    RunThreadsOnIndividual(numportals * 2, true, BasePortalVis);

    //	RunThreadsOnIndividual (numportals*2, true, BetterPortalVis);

    SortPortals();

    CalcPortalVis();

    //
    // assemble the leaf vis lists by oring and compressing the portal lists
    //
    for (i = 0; i < portalclusters; i++)
        ClusterMerge(i);

    printf("Average clusters visible: %i\n", totalvis / portalclusters);
}

void SetPortalSphere(portal_t *p) {
    int32_t i;
    vec3_t total, dist;
    winding_t *w;
    float r, bestr;

    w = p->winding;
    VectorCopy(vec3_origin, total);
    for (i = 0; i < w->numpoints; i++) {
        VectorAdd(total, w->points[i], total);
    }

    for (i = 0; i < 3; i++)
        total[i] /= w->numpoints;

    bestr = 0;
    for (i = 0; i < w->numpoints; i++) {
        VectorSubtract(w->points[i], total, dist);
        r = VectorLength(dist);
        if (r > bestr)
            bestr = r;
    }
    VectorCopy(total, p->origin);
    p->radius = bestr;
}

/*
============
LoadPortals
============
*/
void LoadPortals(char *name) {
    int32_t i, j;
    portal_t *p;
    leaf_t *l;
    char magic[80];
    FILE *f;
    int32_t numpoints;
    winding_t *w;
    int32_t leafnums[2];
    plane_t plane;

    if (!strcmp(name, "-"))
        f = stdin;
    else {
        f = fopen(name, "r");
        if (!f)
            Error("LoadPortals: couldn't read %s\n", name);
    }

    if (fscanf(f, "%79s\n%i\n%i\n", magic, &portalclusters, &numportals) != 3)
        Error("LoadPortals: failed to read header");
    if (strcmp(magic, PORTALFILE))
        Error("LoadPortals: not a portal file");

    printf("%4i portalclusters\n", portalclusters);
    printf("%4i numportals\n", numportals);

    // these counts should take advantage of 64 bit systems automatically
    leafbytes   = ((portalclusters + 63) & ~63) >> 3;
    leaflongs   = leafbytes / sizeof(long);

    portalbytes = ((numportals * 2 + 63) & ~63) >> 3;
    portallongs = portalbytes / sizeof(long);

    // each file portal is split into two memory portals
    portals     = malloc(2 * numportals * sizeof(portal_t));
    memset(portals, 0, 2 * numportals * sizeof(portal_t));

    leafs = malloc(portalclusters * sizeof(leaf_t));
    memset(leafs, 0, portalclusters * sizeof(leaf_t));

    originalvismapsize = portalclusters * leafbytes;
    uncompressedvis    = malloc(originalvismapsize);

    vismap = vismap_p = dvisdata;
    dvis->numclusters = portalclusters;
    vismap_p          = (byte *)&dvis->bitofs[portalclusters];

    vismap_end        = vismap + MAX_MAP_VISIBILITY_QBSP;

    for (i = 0, p = portals; i < numportals; i++) {
        if (fscanf(f, "%i %i %i ", &numpoints, &leafnums[0], &leafnums[1]) != 3)
            Error("LoadPortals: reading portal %i", i);
        if (numpoints > MAX_POINTS_ON_WINDING)
            Error("LoadPortals: portal %i has too many points", i);
        if ((unsigned)leafnums[0] > portalclusters || (unsigned)leafnums[1] > portalclusters)
            Error("LoadPortals: reading portal %i", i);

        w = p->winding = NewWinding(numpoints);
        w->original    = true;
        w->numpoints   = numpoints;

        for (j = 0; j < numpoints; j++) {
            double v[3];
            int32_t k;

            // scanf into double, then assign to vec_t
            // so we don't care what size vec_t is
            if (fscanf(f, "(%lf %lf %lf ) ", &v[0], &v[1], &v[2]) != 3)
                Error("LoadPortals: reading portal %i", i);
            for (k = 0; k < 3; k++)
                w->points[j][k] = v[k];
        }
        fscanf(f, "\n");

        // calc plane
        PlaneFromWinding(w, &plane);

        // create forward portal
        l = &leafs[leafnums[0]];
        if (l->numportals == MAX_PORTALS_ON_LEAF)
            Error("Leaf with too many portals");
        l->portals[l->numportals] = p;
        l->numportals++;

        p->winding = w;
        VectorSubtract(vec3_origin, plane.normal, p->plane.normal);
        p->plane.dist = -plane.dist;
        p->leaf       = leafnums[1];
        SetPortalSphere(p);
        p++;

        // create backwards portal
        l = &leafs[leafnums[1]];
        if (l->numportals == MAX_PORTALS_ON_LEAF)
            Error("Leaf with too many portals");
        l->portals[l->numportals] = p;
        l->numportals++;

        p->winding            = NewWinding(w->numpoints);
        p->winding->numpoints = w->numpoints;
        for (j = 0; j < w->numpoints; j++) {
            VectorCopy(w->points[w->numpoints - 1 - j], p->winding->points[j]);
        }

        p->plane = plane;
        p->leaf  = leafnums[0];
        SetPortalSphere(p);
        p++;
    }

    fclose(f);
}

/*
================
CalcPHS

Calculate the PHS (Potentially Hearable Set)
by ORing together all the PVS visible from a leaf
================
*/
void CalcPHS(void) {
    int32_t i, j, k, l, index;
    int32_t bitbyte;
    long *dest, *src;
    byte *scan;
    int32_t count;
    byte uncompressed[MAX_MAP_LEAFS_QBSP / 8];
    byte compressed[MAX_MAP_LEAFS_QBSP / 8];

    printf("Building PHS...\n");

    count = 0;
    for (i = 0; i < portalclusters; i++) {
        scan = uncompressedvis + i * leafbytes;
        memcpy(uncompressed, scan, leafbytes);
        for (j = 0; j < leafbytes; j++) {
            bitbyte = scan[j];
            if (!bitbyte)
                continue;
            for (k = 0; k < 8; k++) {
                if (!(bitbyte & (1 << k)))
                    continue;
                // OR this pvs row into the phs
                index = ((j << 3) + k);
                if (index >= portalclusters)
                    Error("Bad bit in PVS"); // pad bits should be 0
                src  = (long *)(uncompressedvis + index * leafbytes);
                dest = (long *)uncompressed;
                for (l = 0; l < leaflongs; l++)
                    ((long *)uncompressed)[l] |= src[l];
            }
        }
        for (j = 0; j < portalclusters; j++)
            if (uncompressed[j >> 3] & (1 << (j & 7)))
                count++;

        //
        // compress the bit string
        //
        j    = CompressVis(uncompressed, compressed);

        dest = (long *)vismap_p;
        vismap_p += j;

        if (vismap_p > vismap_end)
            Error("Vismap expansion overflow. Exceeds extended limit");

        dvis->bitofs[i][DVIS_PHS] = (byte *)dest - vismap;

        memcpy(dest, compressed, j);
    }

    printf("Average clusters hearable: %i\n", count / portalclusters);
}

/*
===========
main
===========
*/
int32_t main(int32_t argc, char **argv) {
    char portalfile[1024];
    char source[1024];
    char name[1060];
    int32_t i;

    printf("\n\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< 4vis >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    printf("visibility compiler build " __DATE__ "\n");

    verbose = false;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-threads")) {
            numthreads = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-help")) {
            printf("usage: 4vis [options] [mapname]\n\n"
                   "    -fast: uses 'might see' for a quick loose bound\n"
                   "    -threads #: number of CPU threads to use\n"
                   "    -tmpin: read map from 'tmp' folder\n"
                   "    -tmpout: write map to 'tmp' folder\n"
                   "    -v: extra verbose console output\n\n");
            printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< 4vis HELP >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n\n");
            exit(1);
        } else if (!strcmp(argv[i], "-fast")) {
            printf("fastvis = true\n");
            fastvis = true;
        } else if (!strcmp(argv[i], "-v")) {
            printf("verbose = true\n");
            verbose = true;
        } else if (!strcmp(argv[i], "-nosort")) {
            printf("nosort = true\n");
            nosort = true;
        } else if (!strcmp(argv[i], "-tmpin"))
            strcpy(inbase, "/tmp");
        else if (!strcmp(argv[i], "-tmpout"))
            strcpy(outbase, "/tmp");
        else if (argv[i][0] == '-')
            Error("Unknown option \"%s\"", argv[i]);
        else
            break;
    }

    if (i != argc - 1) {
        printf("usage: 4vis [options] mapfile\n"
               "    -fast                   -help                 -threads #\n"
               "    -tmpin                  -tmpout               -v (verbose)\n\n");
        exit(1);
    }

    ThreadSetDefault();

    SetQdirFromPath(argv[i]);
    strcpy(source, ExpandArg(argv[i]));
    StripExtension(source);
    DefaultExtension(source, ".bsp");

    sprintf(name, "%s%s", inbase, source);
    printf("reading %s\n", name);
    LoadBSPFile(name);
    if (numnodes == 0 || numfaces == 0)
        Error("Empty map");

    sprintf(portalfile, "%s%s", inbase, ExpandArg(argv[i]));
    StripExtension(portalfile);
    strcat(portalfile, ".prt");

    printf("reading %s\n", portalfile);
    LoadPortals(portalfile);

    CalcVis();

    CalcPHS();

    visdatasize = vismap_p - dvisdata;

    printf("visdatasize: %i compressed from %i\n", visdatasize, originalvismapsize * 2);

    if (!use_qbsp && vismap_p > (vismap + DEFAULT_MAP_VISIBILITY))
        printf("\nWARNING: visdatasize exceeds default limit of %i\n\n", DEFAULT_MAP_VISIBILITY);

    sprintf(name, "%s%s", outbase, source);
    WriteBSPFile(name);

    PrintBSPFileSizes();

    printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< END 4vis >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n\n");
    return 0;
}
