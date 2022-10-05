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

#include "qrad.h"

/*

NOTES
-----

every surface must be divided into at least two patches each axis

*/

patch_t ** face_patches;//[MAX_MAP_FACES_QBSP];
entity_t ** face_entity;//[MAX_MAP_FACES_QBSP];
patch_t * patches;//[MAX_PATCHES_QBSP];
unsigned num_patches;
int32_t num_smoothing; // qb: number of phong hits

vec3_t * radiosity;//[MAX_PATCHES_QBSP];    // light leaving a patch
vec3_t * illumination;//[MAX_PATCHES_QBSP]; // light arriving at a patch

vec3_t * face_offset;//[MAX_MAP_FACES_QBSP]; // for rotating bmodels
dplane_t * backplanes;//[MAX_MAP_PLANES_QBSP];

extern char inbase[32], outbase[32];

int32_t fakeplanes; // created planes for origin offset

int32_t numbounce     = 4;     // default was 8
qboolean noblock      = false; // when true, disables occlusion testing on light rays
qboolean extrasamples = false;
qboolean dicepatches  = false;
qboolean noedgefix    = false;
int32_t memory        = false;
float patch_cutoff    = 0.0f; // set with -radmin 0.0..1.0, see MakeTransfers()

float subdiv          = 64;
qboolean dumppatches;

void BuildFaceExtents(void); // qb: from quemap
int32_t TestLine(vec3_t start, vec3_t stop);
float smoothing_threshold; // qb: phong from VHLT
float smoothing_value = DEFAULT_SMOOTHING_VALUE;
float sample_nudge    = DEFAULT_NUDGE_VALUE; // qb: adjustable nudge for multisample

/*
 * 2010-09 Notes
 * These variables are somewhat confusing. The floating point color values are
 *  in the range 0..255 (floating point color should be 0.0 .. 1.0 IMO.)
 *  The following may or may not be precisely correct.
 *  (There are other variables, surface flags, etc., affecting lighting. What
 *   they do, or whether they work at all is "to be determined")
 *
 * see lightmap.c:FinalLightFace()
 *  sequence: ambient is added, lightscale is applied, RGB is "normalized" to
 *  0..255 range, grayscale is applied, RGB is clamped to maxlight.
 *
 * ambient:
 *  set with -ambient option, 0..255 (but only small numbers are useful)
 *  adds the same value to R, G & B
 *  default is 0
 *
 * lightscale:
 *  set with -scale option, 0.0..1.0
 *  scales lightmap globally.
 *
 * saturation:
 *  set with -saturation, 0.0..1.0  //qb: does higher than 1 work?
 *  proportionally saturation texture reflectivity
 *
 * direct_scale:
 *  set with -direct option, 0.0..1.0
 *  controls reflection from emissive surfaces, i.e. brushes with a light value
 *  research indicates it is not the usual practice to include this in
 *    radiosity, so the default should be 0.0. (Would be nice to have this
 *    a per-surface option where surfaces are used like point lights.)
 *
 * entity_scale:
 *  set with -entity option, 0.0..1.0
 *  controls point light radiosity, i.e. light entities.
 *  default is 1.0, no attenuation of point lights
 *
 *
 */
#include <assert.h>
float ambient          = 0.0f;
float lightscale       = 1.0f;
float maxlight         = 255.0f;
// qboolean nocolor = false;
float grayscale        = 0.0f;
float saturation       = 1.0f; // qb: change desaturate to saturation
float direct_scale     = 1.0f;
float entity_scale     = 1.0f;

/*
 * 2010-09 Notes:
 * These are controlled by setting keys in the worldspawn entity. A light
 * entity targeting an info_null entity is used to determine the vector for
 * the sun directional lighting; variable name "sun_pos" is a misnomer, i think.
 * Example:
 * "_sun" "sun_target"  # activates sun
 * "_sun_color" "1.0 1.0 0.0"  # for yellow, sets sun_alt_color true
 * "_sun_light" "50" # light value, variable is sun_main
 * "_sun_ambient" "2" # an ambient light value in the sun color. variable is sun_ambient
 *
 * It might or might not be the case:
 *  if there is no info_null for the light entity with the "sun_target" to
 *  target, then {0,0,0} is used for the target. If _sun_color is not specified
 *  in the .map, the color of the light entity is used.
 */
qboolean sun           = false;
qboolean sun_alt_color = false;
vec3_t sun_pos         = {0.0f, 0.0f, 1.0f};
float sun_main         = 250.0f;
float sun_ambient      = 0.0f;
vec3_t sun_color       = {1, 1, 1};

qboolean nopvs;
qboolean save_trace = false;

extern char source[1024];

/*
===================================================================

MISC

===================================================================
*/

/*
=============
MakeBackplanes
=============
*/
void MakeBackplanes(void) {
    int32_t i;

    for (i = 0; i < numplanes; i++) {
        backplanes[i].dist = -dplanes[i].dist;
        VectorSubtract(vec3_origin, dplanes[i].normal, backplanes[i].normal);
    }
}

int32_t leafparents[MAX_MAP_LEAFS_QBSP];
int32_t nodeparents[MAX_MAP_NODES_QBSP];

/*
=============
MakeParents
=============
*/
void MakeParents(int32_t nodenum, int32_t parent) {
    int32_t i, j;

    nodeparents[nodenum] = parent;

    if (use_qbsp) {
        dnode_tx *node;
        node = &dnodesX[nodenum];
        for (i = 0; i < 2; i++) {
            j = node->children[i];
            if (j < 0)
                leafparents[-j - 1] = nodenum;
            else
                MakeParents(j, nodenum);
        }
    } else {
        dnode_t *node;
        node = &dnodes[nodenum];
        for (i = 0; i < 2; i++) {
            j = node->children[i];
            if (j < 0)
                leafparents[-j - 1] = nodenum;
            else
                MakeParents(j, nodenum);
        }
    }
}

/*
===================================================================

TRANSFER SCALES

===================================================================
*/

int32_t PointInLeafnum(vec3_t point) {
    int32_t nodenum;
    vec_t dist;
    dplane_t *plane;

    nodenum = 0;
    if (use_qbsp) {
        dnode_tx *node;
        while (nodenum >= 0) {
            node  = &dnodesX[nodenum];
            plane = &dplanes[node->planenum];
            dist  = DotProduct(point, plane->normal) - plane->dist;
            if (dist > 0)
                nodenum = node->children[0];
            else
                nodenum = node->children[1];
        }
    } else {
        dnode_t *node;
        while (nodenum >= 0) {
            node  = &dnodes[nodenum];
            plane = &dplanes[node->planenum];
            dist  = DotProduct(point, plane->normal) - plane->dist;
            if (dist > 0)
                nodenum = node->children[0];
            else
                nodenum = node->children[1];
        }
    }

    return -nodenum - 1;
}

dleaf_tx *PointInLeafX(vec3_t point) {
    int32_t num;

    num = PointInLeafnum(point);
    return &dleafsX[num];
}

dleaf_t *MyPointInLeaf(vec3_t point) {
    int32_t num;

    num = PointInLeafnum(point);
    return &dleafs[num];
}

qboolean PvsForOrigin(vec3_t org, byte *pvs) {
    if (!visdatasize) {
        memset(pvs, 255, (numleafs + 7) / 8);
        return true;
    }

    if (use_qbsp) {
        dleaf_tx *leaf;
        leaf = PointInLeafX(org);
        if (leaf->cluster == -1)
            return false; // in solid leaf
        DecompressVis(dvisdata + dvis->bitofs[leaf->cluster][DVIS_PVS], pvs);
    } else {
        dleaf_t *leaf;
        leaf = MyPointInLeaf(org);
        if (leaf->cluster == -1)
            return false; // in solid leaf
        DecompressVis(dvisdata + dvis->bitofs[leaf->cluster][DVIS_PVS], pvs);
    }
    return true;
}

typedef struct tnode_s {
    int32_t type;
    vec3_t normal;
    float dist;
    int32_t children[2];
    int32_t pad;
} tnode_t;

extern tnode_t *tnodes;

int32_t total_transfer;

static long total_mem;

static int32_t first_transfer = 1;

#define MAX_TRACE_BUF ((MAX_PATCHES_QBSP + 7) / 8)

#define TRACE_BYTE(x) (((x) + 7) >> 3)
#define TRACE_BIT(x)  ((x)&0x1F)

static byte trace_buf[MAX_TRACE_BUF + 1];
static byte trace_tmp[MAX_TRACE_BUF + 1];
static int32_t trace_buf_size;

int32_t CompressBytes(int32_t size, byte *source, byte *dest) {
    int32_t j;
    int32_t rep;
    byte *dest_p;

    dest_p = dest + 1;

    for (j = 0; j < size; j++) {
        *dest_p++ = source[j];

        if ((dest_p - dest - 1) >= size) {
            memcpy(dest + 1, source, size);
            dest[0] = 0;
            return size + 1;
        }

        if (source[j])
            continue;

        rep = 1;
        for (j++; j < size; j++)
            if (source[j] || rep == 255)
                break;
            else
                rep++;
        *dest_p++ = rep;

        if ((dest_p - dest - 1) >= size) {
            memcpy(dest + 1, source, size);
            dest[0] = 0;
            return size + 1;
        }

        j--;
    }

    dest[0] = 1;
    return dest_p - dest;
}

void DecompressBytes(int32_t size, byte *in, byte *decompressed) {
    int32_t c;
    byte *out;

    if (in[0] == 0) // not compressed
    {
        memcpy(decompressed, in + 1, size);
        return;
    }

    out = decompressed;
    in++;

    do {
        if (*in) {
            *out++ = *in++;
            continue;
        }

        c = in[1];
        if (!c)
            Error("DecompressBytes: 0 repeat");
        in += 2;
        while (c) {
            *out++ = 0;
            c--;
        }
    } while (out - decompressed < size);
}

static int32_t trace_bytes = 0;

#ifdef WIN32
static inline int32_t lowestCommonNode(int32_t nodeNum1, int32_t nodeNum2)
#else
static inline int32_t lowestCommonNode(int32_t nodeNum1, int32_t nodeNum2)
#endif
{
    int32_t child1, tmp, headNode = 0;

    if (nodeNum1 > nodeNum2) {
        tmp      = nodeNum1;
        nodeNum1 = nodeNum2;
        nodeNum2 = tmp;
    }

re_test:
    // headNode is guaranteed to be <= nodeNum1 and nodeNum1 is < nodeNum2
    if (headNode == nodeNum1)
        return headNode;

    if (use_qbsp) {
        dnode_tx *node;
        child1 = (node = dnodesX + headNode)->children[1];
        if (nodeNum2 < child1)
            // Both nodeNum1 and nodeNum2 are less than child1.
            // In this case, child0 is always a node, not a leaf, so we don't need
            // to check to make sure.
            headNode = node->children[0];
        else if (nodeNum1 < child1)
            // Child1 sits between nodeNum1 and nodeNum2.
            // This means that headNode is the lowest node which contains both
            // nodeNum1 and nodeNum2.
            return headNode;
        else if (child1 > 0)
            // Both nodeNum1 and nodeNum2 are greater than child1.
            // If child1 is a node, that means it contains both nodeNum1 and
            // nodeNum2.
            headNode = child1;
        else
            // Child1 is a leaf, therefore by process of elimination child0 must be
            // a node and must contain boste nodeNum1 and nodeNum2.
            headNode = node->children[0];
        // goto instead of while(1) because it makes the CPU branch predict easier
    } else {
        dnode_t *node;
        child1 = (node = dnodes + headNode)->children[1];
        if (nodeNum2 < child1)
            headNode = node->children[0];
        else if (nodeNum1 < child1)
            return headNode;
        else if (child1 > 0)
            headNode = child1;
        else
            headNode = node->children[0];
    }

    goto re_test;
}

void MakeTransfers(int32_t i) {

    int32_t j;
    vec3_t delta;
    vec_t dist, inv_dist = 0, scale;
    float trans;
    int32_t itrans;
    patch_t *patch, *patch2;
    float total, inv_total;
    dplane_t plane;
    vec3_t origin;
    float transfers[MAX_PATCHES_QBSP];
    int32_t s;
    int32_t itotal;
    byte pvs[(MAX_MAP_LEAFS_QBSP + 7) / 8];
    int32_t cluster;
    int32_t calc_trace, test_trace;

    patch = patches + i;
    total = 0;

    VectorCopy(patch->origin, origin);
    plane = *patch->plane;
    if (!PvsForOrigin(patch->origin, pvs))
        return;

    if (patch->area == 0)
        return;
    // find out which patches will collect light from patch
    patch->numtransfers = 0;
    calc_trace          = (save_trace && memory && first_transfer);
    test_trace          = (save_trace && memory && !first_transfer);

    if (calc_trace) {
        memset(trace_buf, 0, trace_buf_size);
    } else if (test_trace) {
        DecompressBytes(trace_buf_size, patch->trace_hit, trace_buf);
    }
    for (j = 0, patch2 = patches; j < num_patches; j++, patch2++) {
        transfers[j] = 0;

        if (j == i)
            continue;

        if (patch2->area == 0)
            continue;

        // check pvs bit
        if (!nopvs) {
            cluster = patch2->cluster;
            if (cluster == -1)
                continue;
            if (!(pvs[cluster >> 3] & (1 << (cluster & 7))))
                continue; // not in pvs
        }
        if (test_trace && !(trace_buf[TRACE_BYTE(j)] & TRACE_BIT(j)))
            continue;

        // calculate vector
        VectorSubtract(patch2->origin, origin, delta);
        dist = VectorNormalize(delta, delta);

        if (dist == 0) {
            continue;
        } else {
            dist     = sqrt(dist);
            inv_dist = 1.0f / dist;
            delta[0] *= inv_dist;
            delta[1] *= inv_dist;
            delta[2] *= inv_dist;
        }

        // relative angles
        scale = DotProduct(delta, plane.normal);
        scale *= -DotProduct(delta, patch2->plane->normal);
        if (scale <= 0)
            continue;

        // check exact transfer
        trans = scale * patch2->area * inv_dist * inv_dist;

        if (trans > patch_cutoff) {
            if (!test_trace && !noblock &&
                patch2->nodenum != patch->nodenum &&
                TestLine_r(lowestCommonNode(patch->nodenum, patch2->nodenum),
                           patch->origin, patch2->origin)) {
                transfers[j] = 0;
                continue;
            }

            transfers[j] = trans;

            total += trans;
            patch->numtransfers++;
        }
    }

    // copy the transfers out and normalize
    // total should be somewhere near PI if everything went right
    // because partial occlusion isn't accounted for, and nearby
    // patches have underestimated form factors, it will usually
    // be higher than PI
    if (patch->numtransfers) {
        transfer_t *t;

        if (patch->numtransfers < 0 || patch->numtransfers > MAX_PATCHES_QBSP)
            Error("Weird numtransfers");
        s                = patch->numtransfers * sizeof(transfer_t);
        patch->transfers = malloc(s);
        total_mem += s;
        if (!patch->transfers)
            Error("Memory allocation failure");

        //
        // normalize all transfers so all of the light
        // is transfered to the surroundings
        //
        t         = patch->transfers;
        itotal    = 0;
        inv_total = 65536.0f / total;
        for (j = 0; j < num_patches; j++) {
            if (transfers[j] <= 0)
                continue;
            itrans = transfers[j] * inv_total;
            itotal += itrans;
            t->transfer = itrans;
            t->patch    = j;
            t++;
            if (calc_trace) {
                trace_buf[TRACE_BYTE(j)] |= TRACE_BIT(j);
            }
        }
    }

    if (calc_trace) {
        j                = CompressBytes(trace_buf_size, trace_buf, trace_tmp);
        patch->trace_hit = malloc(j);
        memcpy(patch->trace_hit, trace_tmp, j);

        trace_bytes += j;
    }

    // don't bother locking around this.  not that important.
    total_transfer += patch->numtransfers;
}

/*
=============
FreeTransfers
=============
*/
void FreeTransfers(void) {
    int32_t i;

    for (i = 0; i < num_patches; i++) {
        if (!memory) {
            free(patches[i].transfers);
            patches[i].transfers = NULL;
        } else if (patches[i].trace_hit != NULL) {
            free(patches[i].trace_hit);
            patches[i].trace_hit = NULL;
        }
    }
}

//===================================================================

/*
=============
WriteWorld
=============
*/
void WriteWorld(char *name) {
    int32_t i, j;
    FILE *out;
    patch_t *patch;
    winding_t *w;

    out = fopen(name, "w");
    if (!out)
        Error("Couldn't open %s", name);

    for (j = 0, patch = patches; j < num_patches; j++, patch++) {
        w = patch->winding;
        fprintf(out, "%i\n", w->numpoints);
        for (i = 0; i < w->numpoints; i++) {
            fprintf(out, "%5.2f %5.2f %5.2f %5.3f %5.3f %5.3f\n",
                    w->p[i][0],
                    w->p[i][1],
                    w->p[i][2],
                    patch->totallight[0],
                    patch->totallight[1],
                    patch->totallight[2]);
        }
        fprintf(out, "\n");
    }

    fclose(out);
}

//==============================================================

/*
=============
CollectLight
=============
*/
float CollectLight(void) {
    int32_t i, j;
    patch_t *patch;
    vec_t total;

    total = 0;

    for (i = 0, patch = patches; i < num_patches; i++, patch++) {
        // skys never collect light, it is just dropped
        if (patch->sky) {
            VectorClear(radiosity[i]);
            VectorClear(illumination[i]);
            continue;
        }

        for (j = 0; j < 3; j++) {
            patch->totallight[j] += illumination[i][j] / patch->area;
            radiosity[i][j] = illumination[i][j] * patch->reflectivity[j];
        }

        total += radiosity[i][0] + radiosity[i][1] + radiosity[i][2];
        VectorClear(illumination[i]);
    }

    return total;
}

/*
=============
ShootLight

Send light out to other patches
  Run multi-threaded
=============
*/
int32_t c_progress;
int32_t p_progress;
void ShootLight(int32_t patchnum) {
    int32_t k, l;
    transfer_t *trans;
    int32_t num;
    patch_t *patch;
    vec3_t send;

    // this is the amount of light we are distributing
    // prescale it so that multiplying by the 16 bit
    // transfer values gives a proper output value
    for (k = 0; k < 3; k++)
        send[k] = radiosity[patchnum][k] / 0x10000;
    patch = &patches[patchnum];
    if (memory) {
        c_progress = 10 * patchnum / num_patches;

        if (c_progress != p_progress) {
            printf("%i...", c_progress);
            p_progress = c_progress;
        }

        MakeTransfers(patchnum);
    }

    trans = patch->transfers;
    num   = patch->numtransfers;

    for (k = 0; k < num; k++, trans++) {
        for (l = 0; l < 3; l++)
            illumination[trans->patch][l] += send[l] * trans->transfer;
    }
    if (memory) {
        free(patches[patchnum].transfers);
        patches[patchnum].transfers = NULL;
    }
}

/*
=============
BounceLight
=============
*/
void BounceLight(void) {
    int32_t i, j, start = 0, stop;
    float added;
    char name[64];
    patch_t *p;

    for (i = 0; i < num_patches; i++) {
        p = &patches[i];
        for (j = 0; j < 3; j++) {
            //			p->totallight[j] = p->samplelight[j];
            radiosity[i][j] = p->samplelight[j] * p->reflectivity[j] * p->area;
        }
    }
    if (memory)
        trace_buf_size = (num_patches + 7) / 8;

    for (i = 0; i < numbounce; i++) {
        if (memory) {
            p_progress = -1;
            start      = I_FloatTime();
            printf("[%d remaining]  ", numbounce - i);
            total_mem = 0;
        }
        RunThreadsOnIndividual(num_patches, false, ShootLight);
        first_transfer = 0;
        if (memory) {
            stop = I_FloatTime();
            printf(" (%i)\n", stop - start);
        }
        added = CollectLight();

        qprintf("bounce:%i added:%f\n", i, added);
        if (dumppatches && (i == 0 || i == numbounce - 1)) {
            sprintf(name, "bounce%i.txt", i);
            WriteWorld(name);
        }
    }
}

//==============================================================

void CheckPatches(void) {
    int32_t i;
    patch_t *patch;

    for (i = 0; i < num_patches; i++) {
        patch = &patches[i];
        if (patch->totallight[0] < 0 || patch->totallight[1] < 0 || patch->totallight[2] < 0)
            Error("negative patch totallight\n");
    }
}

/*
=============
RadWorld
=============
*/
void RadWorld(void) {
    if (numnodes == 0 || numfaces == 0)
        Error("Empty map");
    MakeBackplanes();
    MakeParents(0, -1);
    MakeTnodes(&dmodels[0]);

    // turn each face into a single patch
    MakePatches();

    // subdivide patches to a maximum dimension
    SubdividePatches();

    BuildFaceExtents(); // qb: from quetoo
    // create directlights out of patches and lights
    CreateDirectLights();
    PairEdges(); // qb: moved here for phong

    // build initial facelights
    RunThreadsOnIndividual(numfaces, true, BuildFacelights);

    if (numbounce > 0) {
        // build transfer lists
        if (!memory) {
            RunThreadsOnIndividual(num_patches, true, MakeTransfers);
            qprintf("transfer lists: %5.1f megs\n", (float)total_transfer * sizeof(transfer_t) / (1024 * 1024));
        }
        numthreads = 1;

        // spread light around
        BounceLight();

        FreeTransfers();

        CheckPatches();
    } else
        numthreads = 1;

    if (memory) {
        printf("Non-memory conservation would require %4.1f\n",
               (float)(total_mem - trace_bytes) / 1048576.0f);
        printf("    megabytes more memory then currently used\n");
    }

    // blend bounced light into direct light and save
    LinkPlaneFaces();

    lightdatasize = 0;
    RunThreadsOnIndividual(numfaces, true, FinalLightFace);
}

/*
========
main

light modelfile
========
*/


void RAD_ProcessArgument(const char * arg) {
    double start, end;
    char name[1060];

    face_patches = (patch_t **)malloc(sizeof(*face_patches) * MAX_MAP_FACES_QBSP);
    face_entity = (entity_t **)malloc(sizeof(*face_entity) * MAX_MAP_FACES_QBSP);
    patches = (patch_t *)malloc(sizeof(*patches) * MAX_PATCHES_QBSP);

    radiosity = (vec3_t *)malloc(sizeof(*radiosity) * MAX_PATCHES_QBSP);
    illumination = (vec3_t *)malloc(sizeof(*illumination) * MAX_PATCHES_QBSP);

    face_offset = (vec3_t *)malloc(sizeof(*face_offset) * MAX_MAP_FACES_QBSP);
    backplanes = (dplane_t *)malloc(sizeof(*backplanes) * MAX_MAP_PLANES_QBSP);

    start               = I_FloatTime();

    strcpy(source, ExpandArg(arg));

    StripExtension(source);
    DefaultExtension(source, ".bsp");

    //	ReadLightFile ();

    sprintf(name, "%s%s", inbase, source);
    printf("reading %s\n", name);
    LoadBSPFile(name);
    if (use_qbsp) {
        maxdata = MAX_MAP_LIGHTING_QBSP;
        step    = QBSP_LMSTEP;
    }
    ParseEntities();
    CalcTextureReflectivity();

    if (!visdatasize) {
        printf("No vis information, direct lighting only.\n");
        numbounce = 0;
        ambient   = 0.1;
    }

    RadWorld();

    if (smoothing_threshold > 0.0) {
        printf("Smoothing edges found: %i\n", num_smoothing);
    }

    sprintf(name, "%s%s", outbase, source);
    printf("writing %s\n", name);
    WriteBSPFile(name);

    end = I_FloatTime();
    printf("%5.0f seconds elapsed\n", end - start);
    printf("%i bytes light data used of %i max.\n", lightdatasize, maxdata);

    PrintBSPFileSizes();
    printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< END 4rad >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n\n");

    free(face_patches);
    free(face_entity);
    free(patches);

    free(radiosity);
    free(illumination);

    free(face_offset);
    free(backplanes);
}
