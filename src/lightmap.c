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

#include "stddef.h"
#include "assert.h"
#include "qrad.h"

#define SINGLEMAP      (64 * 64 * 4)
#define QBSP_SINGLEMAP (256 * 256 * 4) // qb: higher res lightmaps
#define AO_SAMPLES 64
#define AO_RADIUS  32.0f // How far the "dirt" reaches in world units

static vec3_t ao_directions[AO_SAMPLES];
static bool ao_directions_initialized = false;

static void InitAODirections(void) {
    if (ao_directions_initialized)
        return;

    const float golden_ratio = 1.61803398875f;
    const float angle_increment = 3.14159f * 2.0f * golden_ratio;

    for (int i = 0; i < AO_SAMPLES; i++) {
        float t = (float)i / (float)AO_SAMPLES;
        float inclination = acosf(sqrtf(1.0f - t));
        float azimuth = angle_increment * i;

        ao_directions[i][0] = sinf(inclination) * cosf(azimuth);
        ao_directions[i][1] = sinf(inclination) * sinf(azimuth);
        ao_directions[i][2] = cosf(inclination);
    }

    ao_directions_initialized = true;
}

typedef struct
{
    dface_t *faces[2];
    dface_tx *facesX[2];
    bool coplanar;
    bool smooth;
    vec_t cos_normals_angle;
    vec3_t interface_normal;
    vec3_t vertex_normal[2];
} edgeshare_t;

edgeshare_t edgeshare[MAX_MAP_EDGES_QBSP];

int32_t facelinks[MAX_MAP_FACES_QBSP];
int32_t planelinks[2][MAX_MAP_PLANES_QBSP];
int32_t maxdata = DEFAULT_MAP_LIGHTING;
vec3_t face_texnormals[MAX_MAP_FACES_QBSP];
float sunradscale = 0.5;
uint8_t *lightdata_ptr;
float dirt_amount = 0.0f;
static int32_t face_lm_mins[MAX_MAP_FACES_QBSP][2];
static int32_t face_lm_size[MAX_MAP_FACES_QBSP][2];

// qb: quemap- face extents
typedef struct face_extents_s {
    vec3_t mins, maxs;
    vec3_t center;
    vec_t st_mins[2], st_maxs[2];
} face_extents_t;

static face_extents_t face_extents[MAX_MAP_FACES_QBSP];

const dplane_t *getPlaneFromFaceNumber(const uint32_t faceNumber) {
    if (use_qbsp) {
        dface_tx *face = &dfacesX[faceNumber];
        if (face->side) {
            return &backplanes[face->planenum];
        } else {
            return &dplanes[face->planenum];
        }
    } else {
        dface_t *face = &dfaces[faceNumber];
        if (face->side) {
            return &backplanes[face->planenum];
        } else {
            return &dplanes[face->planenum];
        }
    }
}

bool GetIntertexnormal(int32_t facenum1, int32_t facenum2) {
    vec3_t normal;
    const dplane_t *p1 = getPlaneFromFaceNumber(facenum1);
    const dplane_t *p2 = getPlaneFromFaceNumber(facenum2);
    VectorAdd(face_texnormals[facenum1], face_texnormals[facenum2], normal);
    if (!VectorNormalize(normal, normal) || DotProduct(normal, p1->normal) <= NORMAL_EPSILON || DotProduct(normal, p2->normal) <= NORMAL_EPSILON) {
        return false;
    }

    return true;
}

/*
Populates face_extents for all d_bsp_face_t, prior to light creation.
This is done so that sample positions may be nudged outward along
the face normal and towards the face center to help with traces.
 */
void BuildFaceExtents(void) {
    const dvertex_t *v;
    int32_t i, j, k;
    vec_t *mins, *maxs, *center, *st_mins, *st_maxs;

    if (use_qbsp)
        for (k = 0; k < numfaces; k++) {

            const dface_tx *s       = &dfacesX[k];
            const texinfo_t *tex    = &texinfo[s->texinfo];
            const size_t face_index = (ptrdiff_t)(s - dfacesX);

            mins             = face_extents[face_index].mins;
            maxs             = face_extents[face_index].maxs;
            center           = face_extents[face_index].center;
            st_mins          = face_extents[face_index].st_mins;
            st_maxs          = face_extents[face_index].st_maxs;

            mins[0] = mins[1] = BOGUS_RANGE;
            maxs[0] = maxs[1] = -BOGUS_RANGE;

            for (i = 0; i < s->numedges; i++) {
                const int32_t e = dsurfedges[s->firstedge + i];
                if (e >= 0) {
                    v = dvertexes + dedgesX[e].v[0];
                } else {
                    v = dvertexes + dedgesX[-e].v[1];
                }

                for (j = 0; j < 3; j++) // calculate mins, maxs
                {
                    if (v->point[j] > maxs[j]) {
                        maxs[j] = v->point[j];
                    }
                    if (v->point[j] < mins[j]) {
                        mins[j] = v->point[j];
                    }
                }

                /* qb:  from ericw-tools light/ltface.cc:
                 * The (long double) casts below are important: The original code
                 * was written for x87 floating-point which uses 80-bit floats for
                 * intermediate calculations. But if you compile it without the
                 * casts for modern x86_64, the compiler will round each
                 * intermediate result to a 32-bit float, which introduces extra
                 * rounding error.
                 *
                 * This becomes a problem if the rounding error causes the light
                 * utilities and the engine to disagree about the lightmap size
                 * for some surfaces.
                 *
                 * Casting to (long double) keeps the intermediate values at at
                 * least 64 bits of precision, probably 128.
                 */

                for (j = 0; j < 2; j++) // calculate st_mins, st_maxs
                {
                    // const vec_t val = DotProduct(v->point, tex->vecs[j]) + tex->vecs[j][3];
                    const vec_t val = (long double)v->point[0] * tex->vecs[j][0] +
                                      (long double)v->point[1] * tex->vecs[j][1] +
                                      (long double)v->point[2] * tex->vecs[j][2] +
                                      tex->vecs[j][3];
                    if (val < st_mins[j]) {
                        st_mins[j] = val;
                    }
                    if (val > st_maxs[j]) {
                        st_maxs[j] = val;
                    }
                }
            }

            for (i = 0; i < 3; i++) // calculate center
            {
                center[i] = (mins[i] + maxs[i]) / 2.0;
            }
        }
    else // ibsp
        for (k = 0; k < numfaces; k++) {
            const dface_t *s        = &dfaces[k];
            const texinfo_t *tex    = &texinfo[s->texinfo];
            const size_t face_index = (ptrdiff_t)(s - dfaces);

            mins             = face_extents[face_index].mins;
            maxs             = face_extents[face_index].maxs;
            center           = face_extents[face_index].center;
            st_mins          = face_extents[face_index].st_mins;
            st_maxs          = face_extents[face_index].st_maxs;

            mins[0] = mins[1] = BOGUS_RANGE;
            maxs[0] = maxs[1] = -BOGUS_RANGE;

            for (i = 0; i < s->numedges; i++) {
                const int32_t e = dsurfedges[s->firstedge + i];
                if (e >= 0) {
                    v = dvertexes + dedges[e].v[0];
                } else {
                    v = dvertexes + dedges[-e].v[1];
                }

                for (j = 0; j < 3; j++) // calculate mins, maxs
                {
                    if (v->point[j] > maxs[j]) {
                        maxs[j] = v->point[j];
                    }
                    if (v->point[j] < mins[j]) {
                        mins[j] = v->point[j];
                    }
                }

                for (j = 0; j < 2; j++) // calculate st_mins, st_maxs
                {
                    // const vec_t val = DotProduct(v->point, tex->vecs[j]) + tex->vecs[j][3];
                    const vec_t val = (long double)v->point[0] * tex->vecs[j][0] +
                                      (long double)v->point[1] * tex->vecs[j][1] +
                                      (long double)v->point[2] * tex->vecs[j][2] +
                                      tex->vecs[j][3];
                    if (val < st_mins[j]) {
                        st_mins[j] = val;
                    }
                    if (val > st_maxs[j]) {
                        st_maxs[j] = val;
                    }
                }

            }

            for (i = 0; i < 3; i++) // calculate center
            {
                center[i] = (mins[i] + maxs[i]) / 2.0;
            }
        }
}
/*
============
LinkPlaneFaces
============
*/
void LinkPlaneFaces(void) {
    int32_t i;

    if (use_qbsp) {
        dface_tx *f;
        f = dfacesX;
        for (i = 0; i < numfaces; i++, f++) {
            facelinks[i]                     = planelinks[f->side][f->planenum];
            planelinks[f->side][f->planenum] = i;
        }
    } else {
        dface_t *f;
        f = dfaces;
        for (i = 0; i < numfaces; i++, f++) {
            facelinks[i]                     = planelinks[f->side][f->planenum];
            planelinks[f->side][f->planenum] = i;
        }
    }
}

const dplane_t *getPlaneFromFace(const dface_t *face) {
    if (!face) {
        Error("getPlaneFromFace face was NULL\n");
    }

    if (face->side) {
        return &backplanes[face->planenum];
    } else {
        return &dplanes[face->planenum];
    }
}

const dplane_t *getPlaneFromFaceX(const dface_tx *face) {
    if (!face) {
        Error("getPlaneFromFaceX face was NULL\n");
    }

    if (face->side) {
        return &backplanes[face->planenum];
    } else {
        return &dplanes[face->planenum];
    }
}

#define SPATIAL_HASH_SIZE 16384
#define WELD_TOLERANCE    0.02f

typedef struct spatial_node_s {
    int32_t facenum;
    vec3_t point;
    vec3_t face_normal;
    struct spatial_node_s *next;
} spatial_node_t;

spatial_node_t *vertex_hash[SPATIAL_HASH_SIZE];

// Global cache for our spatially smoothed normals
vec3_t *face_vertex_normals[MAX_MAP_FACES_QBSP];

// Fast spatial hash function based on 16-unit grid cells
int32_t GetSpatialHash(vec3_t pos) {
    int32_t x = (int32_t)(pos[0] / 16.0f);
    int32_t y = (int32_t)(pos[1] / 16.0f);
    int32_t z = (int32_t)(pos[2] / 16.0f);
    return abs((x * 73856093) ^ (y * 19349663) ^ (z * 83492791)) % SPATIAL_HASH_SIZE;
}

winding_t *WindingFromFacenum(int32_t facenum) {
    int32_t i, e, firstedge, numedges;
    winding_t *w;

    if (use_qbsp) {
        const dface_tx *fx = dfacesX + facenum;
        firstedge = fx->firstedge;
        numedges  = fx->numedges;
    } else {
        const dface_t *fi = dfaces + facenum;
        firstedge = fi->firstedge;
        numedges  = fi->numedges;
    }

    w = AllocWinding(numedges); 
    w->numpoints = numedges;

    for (i = 0; i < numedges; i++) {
        e = dsurfedges[firstedge + i];
        if (use_qbsp) {
            if (e >= 0) {
                VectorCopy(dvertexes[dedgesX[e].v[0]].point, w->p[i]);
            } else {
                VectorCopy(dvertexes[dedgesX[-e].v[1]].point, w->p[i]);
            }
        } else {
            if (e >= 0) {
                VectorCopy(dvertexes[dedges[e].v[0]].point, w->p[i]);
            } else {
                VectorCopy(dvertexes[dedges[-e].v[1]].point, w->p[i]);
            }
        }
        VectorAdd(w->p[i], face_offset[facenum], w->p[i]);
    }
    return w;
}

/*
Replaces PairEdges. Builds a spatial hash of all winding vertices 
and spatially averages normals within the WELD_TOLERANCE.
 */
void BuildSpatialNormals(void) {
    int32_t i, j, hash, x, y, z;
    spatial_node_t *node;
    winding_t *w;
    int total_smoothed = 0;

    memset(vertex_hash, 0, sizeof(vertex_hash));

    // PASS 1: Populate Hash with UNIT normals (No Area Weighting)
    for (i = 0; i < numfaces; i++) {
        w = WindingFromFacenum(i);
        vec3_t face_normal;
        VectorCopy(getPlaneFromFaceNumber(i)->normal, face_normal);

        for (j = 0; j < w->numpoints; j++) {
            node = malloc(sizeof(spatial_node_t));
            node->facenum = i;
            VectorCopy(w->p[j], node->point);
            VectorCopy(face_normal, node->face_normal); // UNIT WEIGHT
            
            hash = GetSpatialHash(node->point);
            node->next = vertex_hash[hash];
            vertex_hash[hash] = node;
        }
        FreeWinding(w);
    }

    // PASS 2: Smoothing with Multi-Bucket Search
    for (i = 0; i < numfaces; i++) {
        w = WindingFromFacenum(i);
        vec3_t face_normal;
        VectorCopy(getPlaneFromFaceNumber(i)->normal, face_normal);
        face_vertex_normals[i] = malloc(sizeof(vec3_t) * w->numpoints);

        for (j = 0; j < w->numpoints; j++) {
            vec3_t accum_normal;
            VectorCopy(face_normal, accum_normal); 

            // Search the immediate area (3x3x3 grid of buckets) 
            // to ensure vertices on grid lines aren't missed.
            for (x = -1; x <= 1; x++) {
                for (y = -1; y <= 1; y++) {
                    for (z = -1; z <= 1; z++) {
                        vec3_t search_pos;
                        search_pos[0] = w->p[j][0] + (x * 16.0f);
                        search_pos[1] = w->p[j][1] + (y * 16.0f);
                        search_pos[2] = w->p[j][2] + (z * 16.0f);
                        
                        hash = GetSpatialHash(search_pos);
                        
                        for (node = vertex_hash[hash]; node; node = node->next) {
                            if (node->facenum == i) continue;

                            vec3_t delta;
                            VectorSubtract(w->p[j], node->point, delta);
                            if (VectorLength(delta) < WELD_TOLERANCE) {
                                float dot = DotProduct(face_normal, node->face_normal);
                                if (dot >= smoothing_threshold) {
                                    VectorAdd(accum_normal, node->face_normal, accum_normal);
                                    total_smoothed++;
                                }
                            }
                        }
                    }
                }
            }
            VectorNormalize(accum_normal, face_vertex_normals[i][j]);
        }
        FreeWinding(w);
    }
    printf("Total vertex connections smoothed: %i\n", total_smoothed);
}

/*
=================================================================

  POINT TRIANGULATION

=================================================================
*/

typedef struct triedge_s {
    int32_t p0, p1;
    vec3_t normal;
    vec_t dist;
    struct triangle_s *tri;
} triedge_t;

typedef struct triangle_s {
    triedge_t *edges[3];
} triangle_t;

#define MAX_TRI_POINTS 4096  //qb: was 1024
#define MAX_TRI_EDGES  (MAX_TRI_POINTS * 6)
#define MAX_TRI_TRIS   (MAX_TRI_POINTS * 2)

typedef struct
{
    int32_t numpoints;
    int32_t numedges;
    int32_t numtris;
    dplane_t *plane;
    triedge_t *edgematrix[MAX_TRI_POINTS][MAX_TRI_POINTS];
    patch_t *points[MAX_TRI_POINTS];
    triedge_t edges[MAX_TRI_EDGES];
    triangle_t tris[MAX_TRI_TRIS];
} triangulation_t;

/*
===============
AllocTriangulation
===============
*/
triangulation_t *AllocTriangulation(dplane_t *plane) {
    triangulation_t *t;

    t            = malloc(sizeof(triangulation_t));
    t->numpoints = 0;
    t->numedges  = 0;
    t->numtris   = 0;

    t->plane     = plane;

    //	memset (t->edgematrix, 0, sizeof(t->edgematrix));

    return t;
}

/*
===============
FreeTriangulation
===============
*/
void FreeTriangulation(triangulation_t *tr) {
    free(tr);
}

triedge_t *FindEdge(triangulation_t *trian, int32_t p0, int32_t p1) {
    triedge_t *e, *be;
    vec3_t v1;
    vec3_t normal;
    vec_t dist;

    if (trian->edgematrix[p0][p1])
        return trian->edgematrix[p0][p1];

    if (trian->numedges > MAX_TRI_EDGES - 2)
        Error("trian->numedges > MAX_TRI_EDGES-2");

    VectorSubtract(trian->points[p1]->origin, trian->points[p0]->origin, v1);
    VectorNormalize(v1, v1);
    CrossProduct(v1, trian->plane->normal, normal);
    dist   = DotProduct(trian->points[p0]->origin, normal);

    e      = &trian->edges[trian->numedges];
    e->p0  = p0;
    e->p1  = p1;
    e->tri = NULL;
    VectorCopy(normal, e->normal);
    e->dist = dist;
    trian->numedges++;
    trian->edgematrix[p0][p1] = e;

    be                        = &trian->edges[trian->numedges];
    be->p0                    = p1;
    be->p1                    = p0;
    be->tri                   = NULL;
    VectorSubtract(vec3_origin, normal, be->normal);
    be->dist = -dist;
    trian->numedges++;
    trian->edgematrix[p1][p0] = be;

    return e;
}

triangle_t *AllocTriangle(triangulation_t *trian) {
    triangle_t *t;

    if (trian->numtris >= MAX_TRI_TRIS)
        Error("trian->numtris >= MAX_TRI_TRIS");

    t = &trian->tris[trian->numtris];
    trian->numtris++;

    return t;
}

/*
============
TriEdge_r
============
*/
void TriEdge_r(triangulation_t *trian, triedge_t *e) {
    int32_t i, bestp = 0;
    vec3_t v1, v2;
    vec_t *p0, *p1, *p;
    vec_t best, ang;
    triangle_t *nt;

    if (e->tri)
        return; // allready connected by someone

    // find the point with the best angle
    p0   = trian->points[e->p0]->origin;
    p1   = trian->points[e->p1]->origin;
    best = 1.1;
    for (i = 0; i < trian->numpoints; i++) {
        p = trian->points[i]->origin;
        // a 0 dist will form a degenerate triangle
        if (DotProduct(p, e->normal) - e->dist < 0)
            continue; // behind edge
        VectorSubtract(p0, p, v1);
        VectorSubtract(p1, p, v2);
        if (!VectorNormalize(v1, v1))
            continue;
        if (!VectorNormalize(v2, v2))
            continue;
        ang = DotProduct(v1, v2);
        if (ang < best) {
            best  = ang;
            bestp = i;
        }
    }
    if (best >= 1)
        return; // edge doesn't match anything

    // make a new triangle
    nt           = AllocTriangle(trian);
    nt->edges[0] = e;
    nt->edges[1] = FindEdge(trian, e->p1, bestp);
    nt->edges[2] = FindEdge(trian, bestp, e->p0);
    for (i = 0; i < 3; i++)
        nt->edges[i]->tri = nt;
    TriEdge_r(trian, FindEdge(trian, bestp, e->p1));
    TriEdge_r(trian, FindEdge(trian, e->p0, bestp));
}

/*
============
TriangulatePoints
============
*/
void TriangulatePoints(triangulation_t *trian) {
    vec_t d, bestd;
    vec3_t v1;
    int32_t bp1 = 0, bp2 = 0, i, j;
    vec_t *p1, *p2;
    triedge_t *e, *e2;

    if (trian->numpoints < 2)
        return;

    // find the two closest points
    bestd = BOGUS_RANGE;
    for (i = 0; i < trian->numpoints; i++) {
        p1 = trian->points[i]->origin;
        for (j = i + 1; j < trian->numpoints; j++) {
            p2 = trian->points[j]->origin;
            VectorSubtract(p2, p1, v1);
            d = VectorLength(v1);
            if (d < bestd) {
                bestd = d;
                bp1   = i;
                bp2   = j;
            }
        }
    }

    e  = FindEdge(trian, bp1, bp2);
    e2 = FindEdge(trian, bp2, bp1);
    TriEdge_r(trian, e);
    TriEdge_r(trian, e2);
}

/*
===============
AddPointToTriangulation
===============
*/
void AddPointToTriangulation(patch_t *patch, triangulation_t *trian) {
    int32_t pnum;

    pnum = trian->numpoints;
    if (pnum == MAX_TRI_POINTS)
        Error("trian->numpoints == MAX_TRI_POINTS");
    trian->points[pnum] = patch;
    trian->numpoints++;
}

/*
===============
LerpTriangle
===============
*/
void LerpTriangle(triangulation_t *trian, triangle_t *t, vec3_t point, vec3_t color) {
    patch_t *p1, *p2, *p3;
    vec3_t base, d1, d2;
    float x, y, x1, y1;

    p1 = trian->points[t->edges[0]->p0];
    p2 = trian->points[t->edges[1]->p0];
    p3 = trian->points[t->edges[2]->p0];

    VectorCopy(p1->totallight, base);

    x1 = DotProduct(p3->origin, t->edges[0]->normal) - t->edges[0]->dist;
    y1 = DotProduct(p2->origin, t->edges[2]->normal) - t->edges[2]->dist;

    VectorCopy(base, color);

    if (fabs(x1) >= ON_EPSILON) {
        VectorSubtract(p3->totallight, base, d2);
        x = DotProduct(point, t->edges[0]->normal) - t->edges[0]->dist;
        x /= x1;
        VectorMA(color, x, d2, color);
    }
    if (fabs(y1) >= ON_EPSILON) {
        VectorSubtract(p2->totallight, base, d1);
        y = DotProduct(point, t->edges[2]->normal) - t->edges[2]->dist;
        y /= y1;
        VectorMA(color, y, d1, color);
    }
}

bool PointInTriangle(vec3_t point, triangle_t *t) {
    int32_t i;
    triedge_t *e;
    vec_t d;

    for (i = 0; i < 3; i++) {
        e = t->edges[i];
        d = DotProduct(e->normal, point) - e->dist;
        if (d < 0)
            return false; // not inside
    }

    return true;
}

/*
===============
SampleTriangulation
===============
*/
void SampleTriangulation(vec3_t point, triangulation_t *trian, triangle_t **last_valid, vec3_t color) {
    triangle_t *t;
    triedge_t *e;
    vec_t d, best;
    patch_t *p0, *p1;
    vec3_t v1, v2;
    int32_t i, j;

    if (trian->numpoints == 0) {
        VectorClear(color);
        return;
    }

    if (trian->numpoints == 1) {
        VectorCopy(trian->points[0]->totallight, color);
        return;
    }

    // try the last one
    if (*last_valid) {
        if (PointInTriangle(point, *last_valid)) {
            LerpTriangle(trian, *last_valid, point, color);
            return;
        }
    }

    // search for triangles
    for (t = trian->tris, j = 0; j < trian->numtris; t++, j++) {
        if (t == *last_valid)
            continue;

        if (!PointInTriangle(point, t))
            continue;

        *last_valid = t;
        LerpTriangle(trian, t, point, color);
        return;
    }

    // search for exterior edge
    for (e = trian->edges, j = 0; j < trian->numedges; e++, j++) {
        if (e->tri)
            continue; // not an exterior edge

        d = DotProduct(point, e->normal) - e->dist;
        if (d < 0)
            continue; // not in front of edge

        p0 = trian->points[e->p0];
        p1 = trian->points[e->p1];

        VectorSubtract(p1->origin, p0->origin, v1);
        VectorNormalize(v1, v1);
        VectorSubtract(point, p0->origin, v2);
        d = DotProduct(v2, v1);
        if (d < 0)
            continue;
        if (d > 1)
            continue;
        for (i = 0; i < 3; i++)
            color[i] = p0->totallight[i] + d * (p1->totallight[i] - p0->totallight[i]);
        return;
    }

    // search for nearest point
    best = BOGUS_RANGE;
    p1   = NULL;
    for (j = 0; j < trian->numpoints; j++) {
        p0 = trian->points[j];
        VectorSubtract(point, p0->origin, v1);
        d = VectorLength(v1);
        if (d < best) {
            best = d;
            p1   = p0;
        }
    }

    if (!p1)
        Error("SampleTriangulation: no points");

    VectorCopy(p1->totallight, color);
}

/*
=================================================================

  LIGHTMAP SAMPLE GENERATION

=================================================================
*/

typedef struct
{
    vec_t facedist;
    vec3_t facenormal;

    int32_t numsurfpt;
    vec3_t surfpt[QBSP_SINGLEMAP];

    vec3_t modelorg; // for origined bmodels

    vec3_t texorg;
    vec3_t worldtotex[2]; // s = (world - texorg) . worldtotex[0]
    vec3_t textoworld[2]; // world = texorg + s * textoworld[0]

    vec_t exactmins[2], exactmaxs[2];

    int32_t texmins[2], texsize[2];
    int32_t surfnum;
    dface_t *face;
    dface_tx *faceX;
    vec3_t winding_center;
} lightinfo_t;

static lightinfo_t thread_liteinfo[5];
static float *thread_styletable[MAX_LSTYLES];

/*
================
CalcFaceExtents

Fills in s->texmins[] and s->texsize[]
also sets exactmins[] and exactmaxs[]
================
*/
void CalcFaceExtents(lightinfo_t *l) {
    vec_t mins[2], maxs[2], val;
    int32_t i, j, e, map = SINGLEMAP;
    dvertex_t *v;
    texinfo_t *tex;
    vec3_t vt;

    if (use_qbsp) {
        map = QBSP_SINGLEMAP;
        dface_tx *s;
        s       = l->faceX;

        mins[0] = mins[1] = BOGUS_RANGE;
        maxs[0] = maxs[1] = -BOGUS_RANGE;

        tex               = &texinfo[s->texinfo];

        for (i = 0; i < s->numedges; i++) {
            e = dsurfedges[s->firstedge + i];

            if (e >= 0)
                v = dvertexes + dedgesX[e].v[0];
            else
                v = dvertexes + dedgesX[-e].v[1];

            //		VectorAdd (v->point, l->modelorg, vt);
            VectorCopy(v->point, vt);

            for (j = 0; j < 2; j++) {
                val = DotProduct(vt, tex->vecs[j]) + tex->vecs[j][3];
                if (val < mins[j])
                    mins[j] = val;
                if (val > maxs[j])
                    maxs[j] = val;
            }
        }
    } else {
        dface_t *s;
        s       = l->face;

        mins[0] = mins[1] = BOGUS_RANGE;
        maxs[0] = maxs[1] = -BOGUS_RANGE;

        tex               = &texinfo[s->texinfo];

        for (i = 0; i < s->numedges; i++) {
            e = dsurfedges[s->firstedge + i];

            if (e >= 0)
                v = dvertexes + dedges[e].v[0];
            else
                v = dvertexes + dedges[-e].v[1];

            //		VectorAdd (v->point, l->modelorg, vt);
            VectorCopy(v->point, vt);

            for (j = 0; j < 2; j++) {
                val = DotProduct(vt, tex->vecs[j]) + tex->vecs[j][3];
                if (val < mins[j])
                    mins[j] = val;
                if (val > maxs[j])
                    maxs[j] = val;
            }
        }
    }

    for (i = 0; i < 2; i++) {
        l->exactmins[i] = mins[i];
        l->exactmaxs[i] = maxs[i];

        mins[i]         = floor(mins[i] / LMSTEP);
        maxs[i]         = ceil(maxs[i] / LMSTEP);

        l->texmins[i]   = mins[i];
        l->texsize[i]   = maxs[i] - mins[i];
    }

    if (l->texsize[0] * l->texsize[1] > map / 4) // div 4 for extrasamples
    {
        char s[3] = {'X', 'Y', 'Z'};

        for (i = 0; i < 2; i++) {
            printf("Axis: %c\n", s[i]);

            l->exactmins[i] = mins[i];
            l->exactmaxs[i] = maxs[i];

            mins[i]         = floor(mins[i] / LMSTEP);
            maxs[i]         = ceil(maxs[i] / LMSTEP);

            l->texmins[i]   = mins[i];
            l->texsize[i]   = maxs[i] - mins[i];

            printf("  Mins = %10.3f, Maxs = %10.3f,  Size = %10.3f\n", (double)mins[i], (double)maxs[i], (double)(maxs[i] - mins[i]));
        }

        Error("Surface too large to map");
    }
}

/*
================
CalcFaceVectors

Fills in texorg, worldtotex. and textoworld
================
*/
void CalcFaceVectors(lightinfo_t *l) {
    texinfo_t *tex;
    int32_t i, j;
    vec3_t texnormal;
    vec_t distscale;
    vec_t dist, len;
    int32_t w, h;

    if (use_qbsp)
        tex = &texinfo[l->faceX->texinfo];
    else
        tex = &texinfo[l->face->texinfo];

    // convert from float to double
    for (i = 0; i < 2; i++)
        for (j = 0; j < 3; j++)
            l->worldtotex[i][j] = tex->vecs[i][j];

    // calculate a normal to the texture axis.  points can be moved along this
    // without changing their S/T
    texnormal[0] = tex->vecs[1][1] * tex->vecs[0][2] - tex->vecs[1][2] * tex->vecs[0][1];
    texnormal[1] = tex->vecs[1][2] * tex->vecs[0][0] - tex->vecs[1][0] * tex->vecs[0][2];
    texnormal[2] = tex->vecs[1][0] * tex->vecs[0][1] - tex->vecs[1][1] * tex->vecs[0][0];
    VectorNormalize(texnormal, texnormal);

    // flip it towards plane normal
    distscale = DotProduct(texnormal, l->facenormal);
    if (!distscale) {
        qprintf("WARNING: Texture axis perpendicular to face\n");
        distscale = 1;
    }
    if (distscale < 0) {
        distscale = -distscale;
        VectorSubtract(vec3_origin, texnormal, texnormal);
    }

    // distscale is the ratio of the distance along the texture normal to
    // the distance along the plane normal
    distscale = 1 / distscale;

    for (i = 0; i < 2; i++) {
        len  = VectorLength(l->worldtotex[i]);
        dist = DotProduct(l->worldtotex[i], l->facenormal);
        dist *= distscale;
        VectorMA(l->worldtotex[i], -dist, texnormal, l->textoworld[i]);
        VectorScale(l->textoworld[i], (1 / len) * (1 / len), l->textoworld[i]);
    }

    // calculate texorg on the texture plane
    for (i = 0; i < 3; i++)
        l->texorg[i] = -tex->vecs[0][3] * l->textoworld[0][i] - tex->vecs[1][3] * l->textoworld[1][i];

    // project back to the face plane
    dist = DotProduct(l->texorg, l->facenormal) - l->facedist - 1;
    dist *= distscale;
    VectorMA(l->texorg, -dist, texnormal, l->texorg);

    // compensate for org'd bmodels
    VectorAdd(l->texorg, l->modelorg, l->texorg);

    // total sample count
    h            = l->texsize[1] + 1;
    w            = l->texsize[0] + 1;
    l->numsurfpt = w * h;
}

/*
=================
CalcPoints

For each texture aligned grid point, back project onto the plane
to get the world xyz value of the sample point
=================
*/
void CalcPoints(lightinfo_t *l, float sofs, float tofs) {
    int32_t i;
    int32_t s, t, j;
    int32_t w, h;
    vec_t starts, startt, us, ut;
    vec_t *surf;
    vec_t mids, midt;
    vec3_t facemid;

    surf = l->surfpt[0];
    mids = (l->exactmaxs[0] + l->exactmins[0]) / 2;
    midt = (l->exactmaxs[1] + l->exactmins[1]) / 2;

    for (j = 0; j < 3; j++)
        facemid[j] = l->texorg[j] + l->textoworld[0][j] * mids + l->textoworld[1][j] * midt;

    h            = l->texsize[1] + 1;
    w            = l->texsize[0] + 1;
    l->numsurfpt = w * h;

    starts       = l->texmins[0] * LMSTEP;
    startt       = l->texmins[1] * LMSTEP;

    for (t = 0; t < h; t++) {
        for (s = 0; s < w; s++, surf += 3) {
            us = starts + (s + sofs) * LMSTEP;
            ut = startt + (t + tofs) * LMSTEP;

            // if a line can be traced from surf to facemid, the point is good
            for (i = 0; i < 6; i++) {
                // calculate texture point
                for (j = 0; j < 3; j++)
                    surf[j] = l->texorg[j] + l->textoworld[0][j] * us + l->textoworld[1][j] * ut;

                if (use_qbsp) {
                    dleaf_tx *leaf;
                    leaf = RadPointInLeafX(surf);
                    if (leaf->contents != CONTENTS_SOLID) {
                        if (!TestLine_r(0, facemid, surf))
                            break; // got it
                    }
                } else {
                    dleaf_t *leaf;
                    leaf = RadPointInLeaf(surf);
                    if (leaf->contents != CONTENTS_SOLID) {
                        if (!TestLine_r(0, facemid, surf))
                            break; // got it
                    }
                }

                // nudge it
                if (i & 1) {
                    if (us > mids) {
                        us -= 8;
                        if (us < mids)
                            us = mids;
                    } else {
                        us += 8;
                        if (us > mids)
                            us = mids;
                    }
                } else {
                    if (ut > midt) {
                        ut -= 8;
                        if (ut < midt)
                            ut = midt;
                    } else {
                        ut += 8;
                        if (ut > midt)
                            ut = midt;
                    }
                }
            }
        }
    }
}

//==============================================================

#define MAX_STYLES 32
typedef struct facelight_s
{
    int32_t numsamples;
    float *origins;
    int32_t numstyles;
    int32_t stylenums[MAX_STYLES];
    float *samples[MAX_STYLES];
} facelight_t;

directlight_t *directlights[MAX_MAP_LEAFS_QBSP];
facelight_t facelight[MAX_MAP_FACES_QBSP];
int32_t numlights;

/*
==================
Blendlightmaps

Blend sample colors across shared edges to smooth lighting between
adjoining faces and reduce hard seams.
==================
*/
void BlendLightmaps(void) {
    if (blend_amount <= 0.0f)
        return; // disabled

    if (!lightdata_ptr)
        return;
    /* Build edgeshare: map each edge index to up to two adjacent faces so
       Blendlightmaps can iterate face-adjacent pairs without relying on
       external state. */
    memset(edgeshare, 0, sizeof(edgeshare));
    if (use_qbsp) {
        for (int32_t fnum = 0; fnum < numfaces; fnum++) {
            dface_tx *f = &dfacesX[fnum];
            int32_t fe = f->firstedge;
            int32_t ne = f->numedges;
            for (int32_t i = 0; i < ne; i++) {
                int32_t se = dsurfedges[fe + i];
                int32_t ed = se >= 0 ? se : -se;
                if (ed < 0 || ed >= numedges || ed >= MAX_MAP_EDGES_QBSP)
                    continue;
                if (!edgeshare[ed].facesX[0])
                    edgeshare[ed].facesX[0] = f;
                else if (!edgeshare[ed].facesX[1])
                    edgeshare[ed].facesX[1] = f;
            }
        }
    } else {
        for (int32_t fnum = 0; fnum < numfaces; fnum++) {
            dface_t *f = &dfaces[fnum];
            int32_t fe = f->firstedge;
            int32_t ne = f->numedges;
            for (int32_t i = 0; i < ne; i++) {
                int32_t se = dsurfedges[fe + i];
                int32_t ed = se >= 0 ? se : -se;
                if (ed < 0 || ed >= numedges || ed >= MAX_MAP_EDGES_QBSP)
                    continue;
                if (!edgeshare[ed].faces[0])
                    edgeshare[ed].faces[0] = f;
                else if (!edgeshare[ed].faces[1])
                    edgeshare[ed].faces[1] = f;
            }
        }
    }
    int32_t blendcount = 0;
    for (int32_t edgeabs = 0; edgeabs < numedges; edgeabs++) {
        edgeshare_t *es = &edgeshare[edgeabs];
        if (use_qbsp) {
            if (!es->facesX[0] || !es->facesX[1])
                continue;
        } else {
            if (!es->faces[0] || !es->faces[1])
                continue;
        }

        int32_t facenum1;
        int32_t facenum2;
        uint8_t *base1;
        uint8_t *base2;

        if (use_qbsp) {
            facenum1 = (int32_t)(es->facesX[0] - dfacesX);
            facenum2 = (int32_t)(es->facesX[1] - dfacesX);
            base1     = &lightdata_ptr[es->facesX[0]->lightofs];
            base2     = &lightdata_ptr[es->facesX[1]->lightofs];
        } else {
            facenum1 = (int32_t)(es->faces[0] - dfaces);
            facenum2 = (int32_t)(es->faces[1] - dfaces);
            base1     = &lightdata_ptr[es->faces[0]->lightofs];
            base2     = &lightdata_ptr[es->faces[1]->lightofs];
        }

        if (facenum1 < 0 || facenum1 >= numfaces || facenum2 < 0 || facenum2 >= numfaces)
            continue;

        facelight_t *fl1 = &facelight[facenum1];
        facelight_t *fl2 = &facelight[facenum2];

        if (fl1->numsamples <= 0 || fl2->numsamples <= 0)
            continue;
        if (fl1->numstyles <= 0 || fl2->numstyles <= 0)
            continue;

        for (int32_t st1 = 0; st1 < fl1->numstyles; st1++) {
            for (int32_t st2 = 0; st2 < fl2->numstyles; st2++) {
                if (fl1->stylenums[st1] != fl2->stylenums[st2])
                    continue;

                uint8_t *samples1 = base1 + st1 * fl1->numsamples * 3;
                uint8_t *samples2 = base2 + st2 * fl2->numsamples * 3;

                const dplane_t *plane1 = getPlaneFromFaceNumber(facenum1);
                const dplane_t *plane2 = getPlaneFromFaceNumber(facenum2);
                float normal_dot = DotProduct(plane1->normal, plane2->normal);
                if (normal_dot <= 0.0f)
                    continue;

                vec3_t edge_v0, edge_v1, edge_vector;
                float edge_len;
                if (use_qbsp) {
                    const dedge_tx *edge = &dedgesX[edgeabs];
                    VectorCopy(dvertexes[edge->v[0]].point, edge_v0);
                    VectorCopy(dvertexes[edge->v[1]].point, edge_v1);
                } else {
                    const dedge_t *edge = &dedges[edgeabs];
                    VectorCopy(dvertexes[edge->v[0]].point, edge_v0);
                    VectorCopy(dvertexes[edge->v[1]].point, edge_v1);
                }
                VectorSubtract(edge_v1, edge_v0, edge_vector);
                edge_len = VectorLength(edge_vector);

                float angle_factor = 0.5f + 0.5f * normal_dot;
                float edge_radius = edge_len * (0.25f + 0.25f * angle_factor) + 0.5f;
                if (edge_radius > 12.0f) //qb:  clamp to avoid excessive blending
                    edge_radius = 12.0f;
                float edge_radius_sq = edge_radius * edge_radius;

                float face_blend = blend_amount * normal_dot;
                if (face_blend <= 0.0f)
                    continue;

                for (int32_t i = 0; i < fl1->numsamples; i++) {
                    float *origin1 = fl1->origins + i * 3;

                    for (int32_t j = 0; j < fl2->numsamples; j++) {
                        float dx = origin1[0] - fl2->origins[j * 3 + 0];
                        float dy = origin1[1] - fl2->origins[j * 3 + 1];
                        float dz = origin1[2] - fl2->origins[j * 3 + 2];
                        float dist_sq = dx * dx + dy * dy + dz * dz;

                        if (dist_sq > edge_radius_sq)
                            continue;

                        float dist = sqrtf(dist_sq);
                        float falloff = 1.0f - dist / edge_radius;
                        if (falloff <= 0.0f)
                            continue;

                        float alpha = 0.5f * face_blend * falloff;
                        if (alpha <= 0.0f)
                            continue;

                        int32_t idx1 = i * 3;
                        int32_t idx2 = j * 3;
                        for (int32_t c = 0; c < 3; c++) {
                            uint8_t old1 = samples1[idx1 + c];
                            uint8_t old2 = samples2[idx2 + c];
                            samples1[idx1 + c] = (uint8_t)(old1 + alpha * (old2 - old1) + 0.5f);
                            samples2[idx2 + c] = (uint8_t)(old2 + alpha * (old1 - old2) + 0.5f);
                        }
                        blendcount++;
                    }
                }
            }
        }
    }
    if (blend_amount > 0.0f) {
        printf("Blended %d edges\n", blendcount);
    }
}

/*
==================
LightNormals_Process

Collects spatially averaged normals into the NORMALS lump for BSPX support.
Bakes the pooled normals and a per-vertex index list.
==================
*/
void LightNormals_Process(void) {
    if (smoothing_threshold <= 0.0f) 
        return;

    printf("--- LightNormals_Process ---\n");

    size_t total_edges = 0;
    for (int i = 0; i < numfaces; i++) {
        total_edges += use_qbsp ? dfacesX[i].numedges : dfaces[i].numedges;
    }

    vec3_t *pool = malloc(sizeof(vec3_t) * total_edges);
    uint32_t *indices = malloc(sizeof(uint32_t) * total_edges);
    uint32_t num_unique = 0;
    uint32_t current_edge = 0;

    for (int i = 0; i < numfaces; i++) {
        int numedges = use_qbsp ? dfacesX[i].numedges : dfaces[i].numedges;
        vec3_t *v_norms = face_vertex_normals[i];
        
        for (int j = 0; j < numedges; j++) {
            vec3_t n;
            if (v_norms) VectorCopy(v_norms[j], n);
            else VectorCopy(getPlaneFromFaceNumber(i)->normal, n);
            
            int found = -1;
            for (uint32_t k = 0; k < num_unique; k++) {
                if (VectorCompare(pool[k], n)) {
                    found = k; break;
                }
            }
            if (found == -1) {
                VectorCopy(n, pool[num_unique]);
                found = num_unique++;
            }
            indices[current_edge++] = (uint32_t)found;
        }
    }

    bspnormalssize = 4 + (num_unique * 12) + (total_edges * 12);
    if (bspnormalssize > MAX_MAP_LIGHTGRID_QBSP) {
        Error("LightNormals_Process: Normals lump exceeds size limit.");
    }

    uint8_t *ptr = bspnormals;
    memcpy(ptr, &num_unique, 4); ptr += 4;
    memcpy(ptr, pool, num_unique * 12); ptr += num_unique * 12;
    
    for (size_t i = 0; i < total_edges; i++) {
        uint32_t entry[3] = { indices[i], 0, 0 };
        memcpy(ptr, entry, 12);
        ptr += 12;
    }

    free(pool);
    free(indices);
    printf("%i unique normals for %zu edges\n", num_unique, total_edges);
}

/*
==================
FindTargetEntity
==================
*/
entity_t *FindTargetEntity(char *target) {
    int32_t i;
    char *n;

    for (i = 0; i < num_entities; i++) {
        n = ValueForKey(&entities[i], "targetname");
        if (!strcmp(n, target))
            return &entities[i];
    }

    return NULL;
}

//#define	DIRECT_LIGHT	3000
#define DIRECT_LIGHT 3

/*
=============
CreateDirectLights
=============
*/
void CreateDirectLights(void) {
    int32_t i;
    patch_t *p;
    directlight_t *dl;
    dleaf_t *leaf;
    dleaf_tx *leafX;
    int32_t cluster;
    entity_t *e, *e2;
    char *name;
    char *target;
    float angle;
    vec3_t dest;
    char *_color;
    float intensity;
    char *sun_target = NULL;
    char *proc_num;

    //
    // entities
    //
    for (i = 0; i < num_entities; i++) {
        e    = &entities[i];
        name = ValueForKey(e, "classname");
        if (strncmp(name, "light", 5)) {
            if (!strncmp(name, "worldspawn", 10)) {
                sun_target = ValueForKey(e, "_sun");
                if (strlen(sun_target) > 0) {
                    printf("Sun activated.\n");
                    printf("Sky radiosity (sunradscale): %f \n", sunradscale);
                    sun = true;
                }

                proc_num = ValueForKey(e, "_sun_ambient");
                if (strlen(proc_num) > 0) {
                    sun_ambient = atof(proc_num);
                }

                proc_num = ValueForKey(e, "_sun_light");
                if (strlen(proc_num) > 0) {
                    sun_main = atof(proc_num);
                }

                proc_num = ValueForKey(e, "_sun_color");
                if (strlen(proc_num) > 0) {
                    GetVectorForKey(e, "_sun_color", sun_color);

                    sun_alt_color = true;
                    ColorNormalize(sun_color, sun_color);
                }
            }

            continue;
        }

        target = ValueForKey(e, "target");

        if (strlen(target) >= 1 && sun_target && !strcmp(target, sun_target)) // qb: add sun_target check
        {
            vec3_t sun_s, sun_t;
            printf("Sun target found.\n");
            GetVectorForKey(e, "origin", sun_s);

            e2 = FindTargetEntity(target);

            if (!e2) {
                printf("WARNING: sun missing target, 0,0,0 used\n");

                sun_t[0] = 0;
                sun_t[1] = 0;
                sun_t[2] = 0;
            } else {
                GetVectorForKey(e2, "origin", sun_t);
            }

            VectorSubtract(sun_s, sun_t, sun_pos);
            VectorNormalize(sun_pos, sun_pos);
            printf("SUN VECTOR: %f, %f, %f\n", sun_pos[0], sun_pos[1], sun_pos[2]);

            continue;
        }

        numlights++;
        dl = malloc(sizeof(directlight_t));
        memset(dl, 0, sizeof(*dl));

        GetVectorForKey(e, "origin", dl->origin);
        dl->style = FloatForKey(e, "_style");
        if (!dl->style)
            dl->style = FloatForKey(e, "style");
        if (dl->style < 0 || dl->style >= MAX_LSTYLES)
            dl->style = 0;

        dl->nodenum = PointInNodenum(dl->origin);

        if (use_qbsp) {
            leafX   = RadPointInLeafX(dl->origin);
            cluster = leafX->cluster;
        } else {
            leaf    = RadPointInLeaf(dl->origin);
            cluster = leaf->cluster;
        }

        dl->next              = directlights[cluster];
        directlights[cluster] = dl;

        proc_num              = ValueForKey(e, "_wait");
        if (strlen(proc_num) > 0)
            dl->wait = atof(proc_num);
        else {
            proc_num = ValueForKey(e, "wait");

            if (strlen(proc_num) > 0)
                dl->wait = atof(proc_num);
            else
                dl->wait = 1.0f;
        }

        if (dl->wait <= EQUAL_EPSILON)
            dl->wait = 1.0f;

        proc_num = ValueForKey(e, "_angwait");
        if (strlen(proc_num) > 0)
            dl->adjangle = atof(proc_num);
        else
            dl->adjangle = 1.0f;

        // [slipyx] add _falloff
        dl->falloff = atoi(ValueForKey(e, "_falloff"));
        if (dl->falloff < 0)
            dl->falloff = 0;

        intensity = FloatForKey(e, "light");
        if (!intensity)
            intensity = FloatForKey(e, "_light");
        if (!intensity)
            intensity = 300;

        _color = ValueForKey(e, "_color");
        if (_color && _color[0]) {
            sscanf(_color, "%f %f %f", &dl->color[0], &dl->color[1], &dl->color[2]);
            ColorNormalize(dl->color, dl->color);
        } else
            dl->color[0] = dl->color[1] = dl->color[2] = 1.0;

        dl->intensity = intensity * entity_scale;
        dl->type      = emit_point;

        target        = ValueForKey(e, "target");

        if (!strcmp(name, "light_spot") || target[0]) {
            dl->type    = emit_spotlight;
            dl->stopdot = FloatForKey(e, "_cone");
            if (!dl->stopdot)
                dl->stopdot = 20;                          // qb: doubled for new calc
            dl->stopdot = cos(dl->stopdot / 90 * 3.14159); // qb: doubled for new calc
            if (target[0]) {
                // point towards target
                e2 = FindTargetEntity(target);
                if (!e2)
                    printf("WARNING: light at (%i %i %i) has missing target\n",
                           (int32_t)dl->origin[0], (int32_t)dl->origin[1], (int32_t)dl->origin[2]);
                else {
                    GetVectorForKey(e2, "origin", dest);
                    VectorSubtract(dest, dl->origin, dl->normal);
                    VectorNormalize(dl->normal, dl->normal);
                }
            } else {
                // point down angle
                angle = FloatForKey(e, "angle");
                if (angle == ANGLE_UP) {
                    dl->normal[0] = dl->normal[1] = 0;
                    dl->normal[2]                 = 1;
                } else if (angle == ANGLE_DOWN) {
                    dl->normal[0] = dl->normal[1] = 0;
                    dl->normal[2]                 = -1;
                } else {
                    dl->normal[2] = 0;
                    dl->normal[0] = cos(angle / 180 * 3.14159);
                    dl->normal[1] = sin(angle / 180 * 3.14159);
                }
            }
        }
    }

    //
    // surfaces
    //

    for (i = 0, p = patches; i < num_patches; i++, p++) {
        if ((!sun || !p->sky) && p->totallight[0] < DIRECT_LIGHT && p->totallight[1] < DIRECT_LIGHT && p->totallight[2] < DIRECT_LIGHT)
            continue;

        numlights++;
        dl = malloc(sizeof(directlight_t));
        memset(dl, 0, sizeof(*dl));

        VectorCopy(p->origin, dl->origin);

        if (use_qbsp) {
            leafX     = RadPointInLeafX(dl->origin);
            cluster   = leafX->cluster;
            dl->leafX = leafX;
        } else {
            leaf     = RadPointInLeaf(dl->origin);
            cluster  = leaf->cluster;
            dl->leaf = leaf;
        }
        dl->next              = directlights[cluster];
        directlights[cluster] = dl;

        VectorCopy(p->plane->normal, dl->normal);

        if (sun && p->sky) {
            dl->plane     = p->plane;
            dl->type      = emit_sky;
            // qb: for sky radiosity, was dl->intensity = 1.0f;
            dl->intensity = ColorNormalize(p->totallight, dl->color);
            dl->intensity *= p->area * direct_scale;
        } else {
            dl->type      = emit_surface;
            dl->intensity = ColorNormalize(p->totallight, dl->color);
            dl->intensity *= p->area * direct_scale;
        }

        VectorClear(p->totallight); // all sent now
    }

    printf("%i direct lights\n", numlights);
}

static inline int32_t lowestCommonNode(int32_t nodeNum1, int32_t nodeNum2)
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

/*
=============
LightContributionToPoint
=============
*/
static void LightContributionToPoint(directlight_t *l, vec3_t pos, int32_t nodenum,
                                     vec3_t normal, vec3_t color,
                                     float lightscale2,
                                     bool *sun_main_once,
                                     bool *sun_ambient_once) {
    vec3_t delta, target, occluded, colorsky = {0, 0, 0};
    float dot, dot2;
    float dist;
    float scale = 0.0f;
    float main_val;
    int32_t i;
    int32_t lcn;
    bool set_main;

    VectorClear(color);

    VectorSubtract(l->origin, pos, delta);
    dist = VectorNormalize(delta, delta);
    dot  = DotProduct(delta, normal);

    if ((l->type != emit_sky) && (dot <= EQUAL_EPSILON)) // qb: nothing is behind light surface of sky
        return;                                          // behind sample surface

    lcn = lowestCommonNode(nodenum, l->nodenum);
    if (!noblock && TestLine_color(lcn, pos, l->origin, occluded))
        return; // occluded

    if (l->type == emit_sky) {
        // this might be the sun ambient and it might be directional
        set_main = false;
        dot2     = -DotProduct(delta, l->normal);
        if (!*sun_main_once && dot2 > EQUAL_EPSILON) // don't do -extra multisampling on sun
        {

            if (!*sun_ambient_once) // Ambient sky, no -extra multisampling
                scale = sun_ambient;
            else
                scale = 0.0f;

            // Main sky
            dot2 = DotProduct(sun_pos, normal); // sun_pos from target entity
            if (dot2 > EQUAL_EPSILON)           // Main sky
            {
                set_main = true;
                main_val = sun_main * dot2;
                if (!noblock) {
                    if (!RayPlaneIntersect(
                            l->plane->normal, l->plane->dist, pos, sun_pos, target) ||
                        TestLine_color(0, pos, target, occluded)) {
                        set_main = *sun_main_once;
                        main_val = 0.0f;
                    } else {
                        scale += main_val;
                        main_val = 0.0f; // done with it
                    }
                }
            } else {
                if (!*sun_ambient_once) {
                    set_main = false;
                    main_val = 0.0f;
                }
            }
            if (sun_alt_color) // set in .map
                VectorScale(sun_color, scale, colorsky);
            else
                VectorScale(l->color, scale, colorsky);

            *sun_ambient_once = true;
            *sun_main_once    = set_main;
        }
    }
    // else qb: sky radiosity
    {
        switch (l->type) {
        case emit_point:
            // linear falloff
            if (l->falloff == 0)
                scale = (l->intensity - l->wait * dist) * dot; // qb: wait
            // [slipyx] additional falloff behavior, from zzsort/blarghrad
            // inverse
            else if (l->falloff == 1)
                scale = l->intensity / dist * dot;
            // inverse square
            else
                scale = l->intensity / (dist * dist) * dot;
            break;

        case emit_sky: // qb: sky radiosity
            dot2 = -DotProduct(delta, l->normal);

            // qb: disable below, nothing is behind light surface of sky
            //    if (dot2 <= EQUAL_EPSILON)
            //        return;	// behind light surface
            if (!noedgefix) {
                if (dist > 36) // qb: edge lighting fix- don't drop off right away
                    scale = (l->intensity / ((dist - 30) * (dist - 30))) * dot * dot2;
                else if (dist > 16)
                    scale = (l->intensity / (dist - 15)) * dot * dot2;
                else
                    scale = l->intensity * dot * dot2;
            } else
                scale = (l->intensity / (dist * dist)) * dot * dot2;

            scale *= sunradscale; // qb: adjust scale when sun is active
            break;

        case emit_surface:
            dot2 = -DotProduct(delta, l->normal);
            if (dot2 <= EQUAL_EPSILON)
                return; // behind light surface

            if (!noedgefix) {
                if (use_qbsp) {    // qb: 4x lightmap res
                    if (dist > 36) // qb: edge lighting fix- don't drop off right away
                        scale = (l->intensity / ((dist - 15) * (dist - 15))) * dot * dot2;
                    else if (dist > 16)
                        scale = (l->intensity / (dist - 7)) * dot * dot2;
                    else
                        scale = l->intensity * dot * dot2;
                } else {
                    if (dist > 18) // qb: edge lighting fix- don't drop off right away
                        scale = (l->intensity / ((dist - 15) * (dist - 15))) * dot * dot2;
                    else if (dist > 8)
                        scale = (l->intensity / (dist - 7)) * dot * dot2;
                    else
                        scale = l->intensity * dot * dot2;
                }

            } else
                scale = (l->intensity / (dist * dist)) * dot * dot2;
            break;

        case emit_spotlight:
            // linear falloff
            dot2 = -DotProduct(delta, l->normal);
            if (dot2 <= l->stopdot)
                return; // outside light cone
            scale = (l->intensity - l->wait * dist) * dot * powf(dot2, 25.0f) * 15;
            // spot center to surface point attenuation
            // dot2 range is limited, so exponent is big.
            // this term is not really necessary, could have spots with sharp cutoff
            // and for fuzzy edges use multiple lights with different cones.
            break;

        default:
            Error("Bad l->type");
        }
    }

    if (scale > 0.0f) {
        scale *= lightscale2;                       // adjust for multisamples, -extra cmd line arg
        VectorScale(l->color, scale * 0.2, color); // qb: scale hack for intensity similar to original rad
    }

    for (i = 0; i < 3; i++) {
        color[i] += colorsky[i];
        color[i] *= occluded[i];
    }
}

/*              
=============
GatherSampleLight

Lightscale2 is the normalizer for multisampling, -extra cmd line arg
=============
*/

void GatherSampleLight(vec3_t pos, vec3_t normal,
                       float **styletable, int32_t offset, int32_t mapsize, float lightscale2,
                       bool *sun_main_once, bool *sun_ambient_once, uint8_t *pvs, int32_t nodenum,
                       bool have_pvs) {
    int32_t i;
    directlight_t *l;
    float *dest;
    vec3_t color;

    // get the PVS for the pos to limit the number of checks
    if (!have_pvs) {
        if (!PvsForOrigin(pos, pvs)) {
            return;
        }
    }

    for (i = 0; i < dvis->numclusters; i++) {
        if (!(pvs[i >> 3] & (1 << (i & 7))))
            continue;

        for (l = directlights[i]; l; l = l->next) {
            LightContributionToPoint(l, pos, nodenum, normal, color, lightscale2,
                                     sun_main_once, sun_ambient_once);

            // no contribution
            if (VectorCompare(color, vec3_origin))
                continue;

            // if this style doesn't have a table yet, allocate one
            if (!styletable[l->style]) {
                styletable[l->style] = malloc(mapsize);
                memset(styletable[l->style], 0, mapsize);
            }

            dest = styletable[l->style] + offset;
            dest[0] += color[0];
            dest[1] += color[1];
            dest[2] += color[2];
        }
    }
}

/*
=============
AddSampleToPatch

Take the sample's collected light and
add it back into the apropriate patch
for the radiosity pass.

The sample is added to all patches that might include
any part of it.  They are counted and averaged, so it
doesn't generate extra light.
=============
*/

void AddSampleToPatch(vec3_t pos, vec3_t color, int32_t facenum) {
    patch_t *patch;
    vec3_t mins, maxs;
    int32_t i;

    if (numbounce == 0)
        return;
    if (color[0] + color[1] + color[2] < 3.0) // qb: was 3... tried 1... back to 3 
        return;

    for (patch = face_patches[facenum]; patch; patch = patch->next) {
        // see if the point is in this patch (roughly)
        WindingBounds(patch->winding, mins, maxs);
        for (i = 0; i < 3; i++) {
            if (mins[i] > pos[i] + LMSTEP)
                goto nextpatch;
            if (maxs[i] < pos[i] - LMSTEP)
                goto nextpatch;
        }

        // add the sample to the patch
        patch->samples++;
        VectorAdd(patch->samplelight, color, patch->samplelight);
    nextpatch:;
    }
}

/*
Calculates a smoothed normal for a point on a face using spatially welded vertex normals.
spot: The world-space position of the lightmap luxel.
w: The winding (polygon) for this face.
v_normals: The array of pre-calculated smoothed normals for each vertex.
facenormal: The flat plane normal (fallback).
out_normal: The resulting smoothed phong normal.
 */

void GetPhongNormalSpatial(vec3_t spot, winding_t *w, vec3_t *v_normals, vec3_t center,
                             vec3_t facenormal, vec3_t out_normal) {
    int32_t i;
    vec3_t v1, v2, vspot;
    float aa, bb, ab, det, a1, a2;

    // Default to the flat face normal
    VectorCopy(facenormal, out_normal);

    if (!v_normals) return;

    VectorSubtract(spot, center, vspot);

    // Loop through the "pie slices" of the polygon (center to edge)
    for (i = 0; i < w->numpoints; i++) {
        int next = (i + 1) % w->numpoints;

        VectorSubtract(w->p[i], center, v1);
        VectorSubtract(w->p[next], center, v2);

        aa = DotProduct(v1, v1);
        bb = DotProduct(v2, v2);
        ab = DotProduct(v1, v2);
        det = aa * bb - ab * ab;

        // Skip degenerate triangles or parallel vectors
        if (det < 0.001f && det > -0.001f) continue;

        float inv_det = 1.0f / det;
        a1 = (bb * DotProduct(v1, vspot) - ab * DotProduct(v2, vspot)) * inv_det;
        a2 = (aa * DotProduct(v2, vspot) - ab * DotProduct(v1, vspot)) * inv_det;

        // If the sample is within this specific "pie slice" triangle
        if (a1 >= -0.01f && a2 >= -0.01f && (a1 + a2) <= 1.01f) {
            vec3_t temp, blended_normal, edge;
            float a0 = 1.0f - a1 - a2; // Weight for the center (face normal)

            // Longer edges produce stronger phong influence.
            VectorSubtract(w->p[next], w->p[i], edge);
            float edge_len = VectorLength(edge);
            float edge_weight = edge_len / (edge_len + 32.0f);
            float phong_scale = 1.0f + edge_weight;

            // Interpolate: (CenterNormal * a0) + (VertexNormal1 * a1) + (VertexNormal2 * a2)
            VectorScale(facenormal, a0, blended_normal);
            
            VectorScale(v_normals[i], a1 * phong_scale, temp);
            VectorAdd(blended_normal, temp, blended_normal);
            
            VectorScale(v_normals[next], a2 * phong_scale, temp);
            VectorAdd(blended_normal, temp, blended_normal);

            VectorNormalize(blended_normal, out_normal);
            return;
        }
    }
}


//Move the incoming sample position safely towards the true surface center and along the
//surface normal to clear coplanar BSP nodes.

static bool NudgeSamplePosition(const vec3_t in, const vec3_t normal, const vec3_t center,
                                    vec3_t out, uint8_t *pvs) {
    vec3_t dir;
    float dist;

    VectorCopy(in, out);

    //Vector FROM sample TO true geometric center
    VectorSubtract(center, out, dir); 
    dist = VectorLength(dir);
    
    //NaN protection (don't normalize if the sample is already dead-center)
    if (dist > 0.001f) {
        VectorScale(dir, 1.0f / dist, dir);
        // Clamp the inward pull so we don't accidentally push a sample past the center
        float safe_nudge = (sample_nudge < dist) ? sample_nudge : dist;
        VectorMA(out, safe_nudge, dir, out);
    }

    //push off the face plane into empty space to avoid BSP boundary traps
    VectorMA(out, sample_nudge, normal, out);

    return PvsForOrigin(out, pvs);
}

// Variant that only nudges the sample position but does not query the PVS.
// Use when we've already computed a valid PVS for a nearby sample and want
// to avoid the cost of repeated PVS lookups for small jittered offsets.
static inline void NudgeSamplePosition_NoPVS(const vec3_t in, const vec3_t normal, const vec3_t center,
                                             vec3_t out) {
    vec3_t dir;
    float dist;

    VectorCopy(in, out);

    VectorSubtract(center, out, dir);
    dist = VectorLength(dir);
    if (dist > 0.001f) {
        VectorScale(dir, 1.0f / dist, dir);
        float safe_nudge = (sample_nudge < dist) ? sample_nudge : dist;
        VectorMA(out, safe_nudge, dir, out);
    }

    VectorMA(out, sample_nudge, normal, out);
}

/*
Checks if a 3D point lies within the boundaries of a convex winding.
point: The world-space position to test.
w: The winding (polygon) to test against.
normal: The face normal used to derive edge-perpendicular directions.
return true if the point is inside or on the edge (within epsilon), false otherwise.
 */
bool PointInConvexWinding(vec3_t point, winding_t *w, vec3_t normal) {
    int i;
    vec3_t edge, edge_normal, dir;
    float dist;

    for (i = 0; i < w->numpoints; i++) {
        // Create a vector representing the current edge
        VectorSubtract(w->p[(i + 1) % w->numpoints], w->p[i], edge);

        // Calculate a normal for this edge that points inward/outward in the plane
        CrossProduct(normal, edge, edge_normal);

        // Calculate the vector from the edge start to our test point
        VectorSubtract(point, w->p[i], dir);

        // If the dot product is negative, the point is on the "outside" of this edge
        dist = DotProduct(dir, edge_normal);

        // Use a small epsilon (0.1) to tolerate nudged points on or near edges
        if (dist < -0.1f)
            return false;
    }
    return true;
}

//Generates an orthogonal basis (Right, Forward) from a single Normal.
void GenerateBasis(vec3_t normal, vec3_t right, vec3_t forward) {
    vec3_t world_up = {0, 0, 1};
    
    // If the normal is pointing straight up or down, change our reference axis
    if (fabs(normal[2]) > 0.999f) {
        world_up[0] = 1; world_up[1] = 0; world_up[2] = 0;
    }
    
    CrossProduct(world_up, normal, right);
    VectorNormalize(right, right);
    CrossProduct(normal, right, forward);
    VectorNormalize(forward, forward);
}

/* Ambient Occlusion factor for a specific world position.
pos: The world-space position of the luxel.
normal: The smoothed phong normal at this position.
nodenum: The BSP node containing this point (for faster tracing).
return a float between 0.0 (fully occluded/dirty) and 1.0 (unoccluded/bright). */
float CalculateAO(vec3_t pos, vec3_t normal, int32_t nodenum) {
    vec3_t right, forward, ray_start;
    int hits = 0;

    InitAODirections();

    // 1. Generate a TBN basis for the hemisphere
    GenerateBasis(normal, right, forward);

    // 2. Nudge the ray start slightly out from the surface to avoid self-collision
    VectorMA(pos, 0.1f, normal, ray_start);

    for (int i = 0; i < AO_SAMPLES; i++) {
        const vec3_t *dir = &ao_directions[i];
        vec3_t ray_dir;

        // Transform into World Space
        ray_dir[0] = (right[0] * (*dir)[0]) + (forward[0] * (*dir)[1]) + (normal[0] * (*dir)[2]);
        ray_dir[1] = (right[1] * (*dir)[0]) + (forward[1] * (*dir)[1]) + (normal[1] * (*dir)[2]);
        ray_dir[2] = (right[2] * (*dir)[0]) + (forward[2] * (*dir)[1]) + (normal[2] * (*dir)[2]);

        vec3_t ray_end;
        VectorMA(ray_start, AO_RADIUS, ray_dir, ray_end);

        if (TestLine_r(nodenum, ray_start, ray_end)) {
            hits++;
        }
    }

    float factor = 1.0f - ((float)hits / (float)AO_SAMPLES);
    return (factor < 0.0f) ? 0.0f : factor;
}

/*
=============
BuildFacelights
=============
*/
float sampleofs[5][2] =
    {{0, 0}, {-0.25, -0.25}, {0.25, -0.25}, {0.25, 0.25}, {-0.25, 0.25}};

void BuildFacelights(int32_t facenum) {
    lightinfo_t * liteinfo;//[5];
    float **styletable;//[MAX_LSTYLES];
    int32_t i, j;
    float *spot;
    patch_t *patch;
    int32_t numsamples;
    int32_t tablesize;
    facelight_t *fl;
    bool sun_main_once, sun_ambient_once;
    int32_t nodenum;
    vec_t *center;
    vec3_t pos, pointnormal;
    winding_t *w;
    uint8_t pvs[(MAX_MAP_LEAFS_QBSP + 7) / 8];

    /* Use per-invocation/thread-local storage to avoid races when
       BuildFacelights runs concurrently on multiple threads. Allocate
       the potentially large `lightinfo_t` array on the heap to avoid
       overflowing the thread stack. */
    lightinfo_t *local_liteinfo = malloc(sizeof(lightinfo_t) * 5);
    if (!local_liteinfo) {
        Error("BuildFacelights: malloc failed\n");
    }
    float *local_styletable[MAX_LSTYLES];

    liteinfo = local_liteinfo;
    styletable = local_styletable;
    for (i = 0; i < MAX_LSTYLES; i++)
        styletable[i] = NULL;

    if (use_qbsp) {
        dface_tx *this_face;
        this_face = &dfacesX[facenum];

        if (texinfo[this_face->texinfo].flags & (SURF_WARP | SURF_SKY))
            goto cleanup; // non-lit texture

        memset(styletable, 0, sizeof(*styletable) * MAX_LSTYLES);

        if (extrasamples) // set with -extra option
            numsamples = 5;
        else
            numsamples = 1;
        for (i = 0; i < numsamples; i++) {
            memset(&liteinfo[i], 0, sizeof(liteinfo[i]));
            liteinfo[i].surfnum = facenum;
            liteinfo[i].faceX   = this_face;
            VectorCopy(dplanes[this_face->planenum].normal, liteinfo[i].facenormal);
            liteinfo[i].facedist = dplanes[this_face->planenum].dist;
            if (this_face->side) {
                VectorSubtract(vec3_origin, liteinfo[i].facenormal, liteinfo[i].facenormal);
                liteinfo[i].facedist = -liteinfo[i].facedist;
            }

            // get the origin offset for rotating bmodels
            VectorCopy(face_offset[facenum], liteinfo[i].modelorg);

            CalcFaceVectors(&liteinfo[i]);
            CalcFaceExtents(&liteinfo[i]);
            face_lm_mins[facenum][0] = liteinfo[i].texmins[0];
            face_lm_mins[facenum][1] = liteinfo[i].texmins[1];
            face_lm_size[facenum][0] = liteinfo[i].texsize[0];
            face_lm_size[facenum][1] = liteinfo[i].texsize[1];
            CalcPoints(&liteinfo[i], sampleofs[i][0], sampleofs[i][1]);
        }
    } else {
        dface_t *this_face;
        this_face = &dfaces[facenum];

        if (texinfo[this_face->texinfo].flags & (SURF_WARP | SURF_SKY))
            goto cleanup; // non-lit texture

        memset(styletable, 0, sizeof(*styletable) * MAX_LSTYLES);

        if (extrasamples) // set with -extra option
            numsamples = 5;
        else
            numsamples = 1;
        for (i = 0; i < numsamples; i++) {
            memset(&liteinfo[i], 0, sizeof(liteinfo[i]));
            liteinfo[i].surfnum = facenum;
            liteinfo[i].face    = this_face;
            VectorCopy(dplanes[this_face->planenum].normal, liteinfo[i].facenormal);
            liteinfo[i].facedist = dplanes[this_face->planenum].dist;
            if (this_face->side) {
                VectorSubtract(vec3_origin, liteinfo[i].facenormal, liteinfo[i].facenormal);
                liteinfo[i].facedist = -liteinfo[i].facedist;
            }

            // get the origin offset for rotating bmodels
            VectorCopy(face_offset[facenum], liteinfo[i].modelorg);

            CalcFaceVectors(&liteinfo[i]);
            CalcFaceExtents(&liteinfo[i]);
            CalcPoints(&liteinfo[i], sampleofs[i][0], sampleofs[i][1]);
        }
    }
    tablesize     = liteinfo[0].numsurfpt * sizeof(vec3_t);
    styletable[0] = malloc(tablesize);
    memset(styletable[0], 0, tablesize);

    fl             = &facelight[facenum];
    fl->numsamples = liteinfo[0].numsurfpt;
    fl->origins    = malloc(tablesize);
    w = WindingFromFacenum(facenum);

    memcpy(fl->origins, liteinfo[0].surfpt, tablesize);
    center = face_extents[facenum].center; // center of the face

    VectorClear(liteinfo[0].winding_center);
    for (j = 0; j < w->numpoints; j++) {
        VectorAdd(liteinfo[0].winding_center, w->p[j], liteinfo[0].winding_center);
    }
    VectorScale(liteinfo[0].winding_center, 1.0f / w->numpoints, liteinfo[0].winding_center);

    for (i = 0; i < liteinfo[0].numsurfpt; i++) {
        sun_ambient_once = false;
        sun_main_once    = false;
        bool have_pvs = false;
  
        for (j = 0; j < numsamples; j++) {
            if (numsamples > 1) {
                if (j == 0) {
                    // compute a valid nudged position and PVS once, reuse for the jittered offsets
                    if (!NudgeSamplePosition(liteinfo[j].surfpt[i], liteinfo[0].facenormal, center, pos, pvs)) {
                        continue; // not a valid point
                    }
                    have_pvs = true;
                } else {
                    // apply the same nudging logic without the PVS query
                    NudgeSamplePosition_NoPVS(liteinfo[j].surfpt[i], liteinfo[0].facenormal, center, pos);
                }
            } else {
                VectorCopy(liteinfo[j].surfpt[i], pos);
                have_pvs = false;
            }

            nodenum = PointInNodenum(pos);

            if (smoothing_threshold > 0.0 && face_vertex_normals[facenum]) {
                // use the face geometry (w) and vertex normals to interpolate the exact normal at the 'pos' coordinate.
                GetPhongNormalSpatial(pos, w, face_vertex_normals[facenum], liteinfo[0].winding_center,
                                      liteinfo[0].facenormal, pointnormal);
            } else {
                VectorCopy(liteinfo[0].facenormal, pointnormal);
            }
            GatherSampleLight(pos, pointnormal, styletable, i * 3, tablesize, 1.0 / numsamples,
                              &sun_main_once, &sun_ambient_once, pvs, nodenum, have_pvs);
        }

        float ao_factor = 1.0f;
        
        if (dirt_amount) { 
            // Use the smoothed pointnormal 
            ao_factor = CalculateAO(pos, pointnormal, nodenum);
            
            // Apply a contrast curve to the AO to make it look punchier
            ao_factor = dirt_amount * powf(ao_factor, 2.0f); 
        }

        // Apply AO to the gathered light in the styletable
        if (styletable[0]) {
            styletable[0][i * 3 + 0] *= ao_factor;
            styletable[0][i * 3 + 1] *= ao_factor;
            styletable[0][i * 3 + 2] *= ao_factor;
        }

        // contribute the sample to one or more patches
        AddSampleToPatch(liteinfo[0].surfpt[i], styletable[0] + i * 3, facenum);
    }

    // average up the direct light on each patch for radiosity
    for (patch = face_patches[facenum]; patch; patch = patch->next) {
        if (patch->samples) {
            VectorScale(patch->samplelight, 1.0 / patch->samples, patch->samplelight);
        }
    }

    for (i = 0; i < MAX_LSTYLES; i++) {
        if (!styletable[i])
            continue;
        if (fl->numstyles == MAX_STYLES)
            break;
        fl->samples[fl->numstyles]   = styletable[i];
        fl->stylenums[fl->numstyles] = i;
        fl->numstyles++;
    }

    // the light from DIRECT_LIGHTS is sent out, but the
    // texture itself should still be full bright
    if (face_patches[facenum]->baselight[0] >= DIRECT_LIGHT ||
        face_patches[facenum]->baselight[1] >= DIRECT_LIGHT ||
        face_patches[facenum]->baselight[2] >= DIRECT_LIGHT) {
        spot = fl->samples[0];
        for (i = 0; i < liteinfo[0].numsurfpt; i++, spot += 3) {
            VectorAdd(spot, face_patches[facenum]->baselight, spot);
        }
    }

cleanup:
    /* free any allocated style tables and the heap-allocated liteinfo */
    for (i = 0; i < MAX_LSTYLES; i++) {
        if (!styletable || !styletable[i])
            continue;
        /* if this style buffer was moved into the facelight samples, don't free it */
        bool owned = true;
        if (fl) {
            int k;
            owned = true;
            for (k = 0; k < fl->numstyles; k++) {
                if (fl->samples[k] == styletable[i]) {
                    owned = false; /* ownership transferred */
                    break;
                }
            }
        }
        if (owned) {
            free(styletable[i]);
            styletable[i] = NULL;
        }
    }
    if (liteinfo) {
        /* if liteinfo points to our heap allocation, free it */
        if (liteinfo != thread_liteinfo)
            free(liteinfo);
        liteinfo = thread_liteinfo; /* restore global pointer */
    }
    return;
}


/*
==================
DecoupledLM_Process

Bakes the lightmap dimensions and coordinate projection parameters 
into the DECOUPLED_LM lump.
==================
*/
void DecoupledLM_Process(void) {
    printf("--- DecoupledLM_Process ---\n");

    uint8_t *out = decoupledlm;

    for (int i = 0; i < numfaces; i++) {
        int texinfo_idx;
        uint32_t light_ofs;

        if (use_qbsp) {
            texinfo_idx = dfacesX[i].texinfo;
            light_ofs = dfacesX[i].lightofs;
        } else {
            texinfo_idx = dfaces[i].texinfo;
            light_ofs = dfaces[i].lightofs;
        }

        texinfo_t *tex = &texinfo[texinfo_idx];
        
        uint16_t w = (uint16_t)(face_lm_size[i][0] + 1);
        uint16_t h = (uint16_t)(face_lm_size[i][1] + 1);

        memcpy(out, &w, 2); out += 2;
        memcpy(out, &h, 2); out += 2;
        memcpy(out, &light_ofs, 4); out += 4;

        for (int j = 0; j < 2; j++) {
            vec3_t axis;
            // axis = projection vector / LMSTEP
            VectorScale(tex->vecs[j], 1.0f / 16.0f, axis);
            memcpy(out, axis, 12); out += 12;

            // offset = (translation / LMSTEP) - texmins
            float offset = (tex->vecs[j][3] / 16.0f) - (float)face_lm_mins[i][j];
            memcpy(out, &offset, 4); out += 4;
        }
    }

    decoupledlmsize = out - decoupledlm;
    printf("Baked decoupled data for %d faces (%d bytes)\n", numfaces, decoupledlmsize);
}

/*
=============
FinalLightFace

Add the indirect lighting on top of the direct
lighting and save into final map format
=============
*/
void FinalLightFace(int32_t facenum) {
    int32_t i, j, st;
    vec3_t lb;
    patch_t *patch;
    triangulation_t *trian = NULL;
    facelight_t *fl;
    float max;
    float newmax;
    uint8_t *dest;
    triangle_t *last_valid;
    int32_t pfacenum;
    vec3_t facemins, facemaxs;

    fl = &facelight[facenum];

    ThreadLock();
    i = lightdatasize;
    lightdatasize += fl->numstyles * (fl->numsamples * 3);

    if (lightdatasize > maxdata) {
        printf("face %d of %d\n", facenum, numfaces);
        Error("lightdatasize %i > maxdata %i", lightdatasize, maxdata);
    }
    ThreadUnlock();

    if (use_qbsp) {
        dface_tx *f;
        f = &dfacesX[facenum];

        if (texinfo[f->texinfo].flags & (SURF_WARP | SURF_SKY))
            return; // non-lit texture

        f->lightofs  = i;
        f->styles[0] = 0;
        f->styles[1] = f->styles[2] = f->styles[3] = 0xff;

        //
        // set up the triangulation
        //
        if (numbounce > 0) {
            ClearBounds(facemins, facemaxs);
            for (i = 0; i < f->numedges; i++) {
                int32_t ednum;

                ednum = dsurfedges[f->firstedge + i];
                if (ednum >= 0)
                    AddPointToBounds(dvertexes[dedgesX[ednum].v[0]].point,
                                     facemins, facemaxs);
                else
                    AddPointToBounds(dvertexes[dedgesX[-ednum].v[1]].point,
                                     facemins, facemaxs);
            }

            trian = AllocTriangulation(&dplanes[f->planenum]);

            // for all faces on the plane, add the nearby patches
            // to the triangulation
            for (pfacenum = planelinks[f->side][f->planenum]; pfacenum; pfacenum = facelinks[pfacenum]) {
                for (patch = face_patches[pfacenum]; patch; patch = patch->next) {
                    for (i = 0; i < 3; i++) {
                        if (facemins[i] - patch->origin[i] > subdiv * 2)
                            break;
                        if (patch->origin[i] - facemaxs[i] > subdiv * 2)
                            break;
                    }
                    if (i != 3)
                        continue; // not needed for this face
                    AddPointToTriangulation(patch, trian);
                }
            }
            for (i = 0; i < trian->numpoints; i++)
                memset(trian->edgematrix[i], 0, trian->numpoints * sizeof(trian->edgematrix[0][0]));
            TriangulatePoints(trian);
        }

        //
        // sample the triangulation
        //

        dest = &lightdata_ptr[f->lightofs];

        if (fl->numstyles > MAXLIGHTMAPS) {
            fl->numstyles = MAXLIGHTMAPS;
            //	printf ("face with too many lightstyles: (%f %f %f)\n",
            //		face_patches[facenum]->origin[0],
            //		face_patches[facenum]->origin[1],
            //		face_patches[facenum]->origin[2]
            //		);
        }
        for (st = 0; st < fl->numstyles; st++) {
            last_valid    = NULL;
            f->styles[st] = fl->stylenums[st];

            for (j = 0; j < fl->numsamples; j++) {
                VectorCopy((fl->samples[st] + j * 3), lb);
                if (numbounce > 0 && st == 0) {
                    vec3_t add;

                    SampleTriangulation(fl->origins + j * 3, trian, &last_valid, add);
                    VectorAdd(lb, add, lb);
                }

                /*
                 * to allow experimenting, ambient and lightscale are not limited
                 *  to reasonable ranges.
                 */
                if (ambient >= -255.0f && ambient <= 255.0f) {
                    // add fixed white ambient.
                    lb[0] += ambient;
                    lb[1] += ambient;
                    lb[2] += ambient;
                }
                if (lightscale > 0.0f) {
                    // apply lightscale, scale down or up
                    lb[0] *= lightscale;
                    lb[1] *= lightscale;
                    lb[2] *= lightscale;
                }
                // negative values not allowed
                lb[0] = (lb[0] < 0.0f) ? 0.0f : lb[0];
                lb[1] = (lb[1] < 0.0f) ? 0.0f : lb[1];
                lb[2] = (lb[2] < 0.0f) ? 0.0f : lb[2];

                /*			qprintf("{%f %f %f}:",lb[0],lb[1],lb[2]);*/

                // determine max of R,G,B
                max   = lb[0] > lb[1] ? lb[0] : lb[1];
                max   = max > lb[2] ? max : lb[2];

                if (max < 1.0f)
                    max = 1.0f;

                // note that maxlight based scaling is per-sample based on
                //  highest value of R, G, and B
                // adjust for -maxlight option
                newmax = max;
                if (max > maxlight) {
                    newmax = maxlight;
                    newmax /= max; // scaling factor 0.0..1.0
                    // scale into 0.0..maxlight range
                    lb[0] *= newmax;
                    lb[1] *= newmax;
                    lb[2] *= newmax;
                }

                // and output to 8:8:8 RGB
                *dest++ = (uint8_t)(lb[0] + 0.5);
                *dest++ = (uint8_t)(lb[1] + 0.5);
                *dest++ = (uint8_t)(lb[2] + 0.5);
            }
        }
    } else // ibsp
    {
        dface_t *f;
        f = &dfaces[facenum];

        if (texinfo[f->texinfo].flags & (SURF_WARP | SURF_SKY))
            return; // non-lit texture

        f->lightofs  = i;
        f->styles[0] = 0;
        f->styles[1] = f->styles[2] = f->styles[3] = 0xff;

        //
        // set up the triangulation
        //
        if (numbounce > 0) {
            ClearBounds(facemins, facemaxs);
            for (i = 0; i < f->numedges; i++) {
                int32_t ednum;

                ednum = dsurfedges[f->firstedge + i];
                if (ednum >= 0)
                    AddPointToBounds(dvertexes[dedges[ednum].v[0]].point,
                                     facemins, facemaxs);
                else
                    AddPointToBounds(dvertexes[dedges[-ednum].v[1]].point,
                                     facemins, facemaxs);
            }

            trian = AllocTriangulation(&dplanes[f->planenum]);

            // for all faces on the plane, add the nearby patches
            // to the triangulation
            for (pfacenum = planelinks[f->side][f->planenum]; pfacenum; pfacenum = facelinks[pfacenum]) {
                for (patch = face_patches[pfacenum]; patch; patch = patch->next) {
                    for (i = 0; i < 3; i++) {
                        if (facemins[i] - patch->origin[i] > subdiv * 2)
                            break;
                        if (patch->origin[i] - facemaxs[i] > subdiv * 2)
                            break;
                    }
                    if (i != 3)
                        continue; // not needed for this face
                    AddPointToTriangulation(patch, trian);
                }
            }
            for (i = 0; i < trian->numpoints; i++)
                memset(trian->edgematrix[i], 0, trian->numpoints * sizeof(trian->edgematrix[0][0]));
            TriangulatePoints(trian);
        }

        //
        // sample the triangulation
        //

        dest = &lightdata_ptr[f->lightofs];

        if (fl->numstyles > MAXLIGHTMAPS) {
            fl->numstyles = MAXLIGHTMAPS;
            //	printf ("face with too many lightstyles: (%f %f %f)\n",
            //		face_patches[facenum]->origin[0],
            //		face_patches[facenum]->origin[1],
            //		face_patches[facenum]->origin[2]
            //		);
        }
        for (st = 0; st < fl->numstyles; st++) {
            last_valid    = NULL;
            f->styles[st] = fl->stylenums[st];

            for (j = 0; j < fl->numsamples; j++) {
                VectorCopy((fl->samples[st] + j * 3), lb);
                if (numbounce > 0 && st == 0) {
                    vec3_t add;

                    SampleTriangulation(fl->origins + j * 3, trian, &last_valid, add);
                    VectorAdd(lb, add, lb);
                }

                /*
                 * to allow experimenting, ambient and lightscale are not limited
                 *  to reasonable ranges.
                 */
                if (ambient >= -255.0f && ambient <= 255.0f) {
                    // add fixed white ambient.
                    lb[0] += ambient;
                    lb[1] += ambient;
                    lb[2] += ambient;
                }
                if (lightscale > 0.0f) {
                    // apply lightscale, scale down or up
                    lb[0] *= lightscale;
                    lb[1] *= lightscale;
                    lb[2] *= lightscale;
                }
                // negative values not allowed
                lb[0] = (lb[0] < 0.0f) ? 0.0f : lb[0];
                lb[1] = (lb[1] < 0.0f) ? 0.0f : lb[1];
                lb[2] = (lb[2] < 0.0f) ? 0.0f : lb[2];

                /*			qprintf("{%f %f %f}:",lb[0],lb[1],lb[2]);*/

                // determine max of R,G,B
                max   = lb[0] > lb[1] ? lb[0] : lb[1];
                max   = max > lb[2] ? max : lb[2];

                if (max < 1.0f)
                    max = 1.0f;

                // note that maxlight based scaling is per-sample based on
                //  highest value of R, G, and B
                // adjust for -maxlight option
                newmax = max;
                if (max > maxlight) {
                    newmax = maxlight;
                    newmax /= max; // scaling factor 0.0..1.0
                    // scale into 0.0..maxlight range
                    lb[0] *= newmax;
                    lb[1] *= newmax;
                    lb[2] *= newmax;
                }

                // and output to 8:8:8 RGB
                *dest++ = (uint8_t)(lb[0] + 0.5);
                *dest++ = (uint8_t)(lb[1] + 0.5);
                *dest++ = (uint8_t)(lb[2] + 0.5);
            }
        }
    }

    if (numbounce > 0)
        FreeTriangulation(trian);
}

/*
=============
FinalLightFace

Add the indirect lighting on top of the direct
lighting and save into final map format
=============
*/
void FinalLightFaceSH(int32_t facenum) {
    int32_t i, j, st;
    vec3_t lb;
    patch_t *patch;
    triangulation_t *trian = NULL;
    facelight_t *fl;
    float max;
    float newmax;
    uint8_t *dest;
    triangle_t *last_valid;
    int32_t pfacenum;
    vec3_t facemins, facemaxs;

    fl = &facelight[facenum];

    ThreadLock();
    i = lightdatasize;
    lightdatasize += fl->numstyles * (fl->numsamples * 3 * 4);

    if (lightdatasize > maxdata) {
        printf("face %d of %d\n", facenum, numfaces);
        Error("lightdatasize %i > maxdata %i", lightdatasize, maxdata);
    }
    ThreadUnlock();

    if (use_qbsp) {
        dface_tx *f;
        f = &dfacesX[facenum];

        if (texinfo[f->texinfo].flags & (SURF_WARP | SURF_SKY))
            return; // non-lit texture

        f->lightofs  = i;
        f->styles[0] = 0;
        f->styles[1] = f->styles[2] = f->styles[3] = 0xff;

        //
        // set up the triangulation
        //
        if (numbounce > 0) {
            ClearBounds(facemins, facemaxs);
            for (i = 0; i < f->numedges; i++) {
                int32_t ednum;

                ednum = dsurfedges[f->firstedge + i];
                if (ednum >= 0)
                    AddPointToBounds(dvertexes[dedgesX[ednum].v[0]].point,
                                     facemins, facemaxs);
                else
                    AddPointToBounds(dvertexes[dedgesX[-ednum].v[1]].point,
                                     facemins, facemaxs);
            }

            trian = AllocTriangulation(&dplanes[f->planenum]);

            // for all faces on the plane, add the nearby patches
            // to the triangulation
            for (pfacenum = planelinks[f->side][f->planenum]; pfacenum; pfacenum = facelinks[pfacenum]) {
                for (patch = face_patches[pfacenum]; patch; patch = patch->next) {
                    for (i = 0; i < 3; i++) {
                        if (facemins[i] - patch->origin[i] > subdiv * 2)
                            break;
                        if (patch->origin[i] - facemaxs[i] > subdiv * 2)
                            break;
                    }
                    if (i != 3)
                        continue; // not needed for this face
                    AddPointToTriangulation(patch, trian);
                }
            }
            for (i = 0; i < trian->numpoints; i++)
                memset(trian->edgematrix[i], 0, trian->numpoints * sizeof(trian->edgematrix[0][0]));
            TriangulatePoints(trian);
        }

        //
        // sample the triangulation
        //

        dest = &lightdata_ptr[f->lightofs];

        if (fl->numstyles > MAXLIGHTMAPS) {
            fl->numstyles = MAXLIGHTMAPS;
            //	printf ("face with too many lightstyles: (%f %f %f)\n",
            //		face_patches[facenum]->origin[0],
            //		face_patches[facenum]->origin[1],
            //		face_patches[facenum]->origin[2]
            //		);
        }
        for (st = 0; st < fl->numstyles; st++) {
            last_valid    = NULL;
            f->styles[st] = fl->stylenums[st];

            for (j = 0; j < fl->numsamples; j++) {
                VectorCopy((fl->samples[st] + j * 3), lb);
                if (numbounce > 0 && st == 0) {
                    vec3_t add;

                    SampleTriangulation(fl->origins + j * 3, trian, &last_valid, add);
                    VectorAdd(lb, add, lb);
                }

                /*
                 * to allow experimenting, ambient and lightscale are not limited
                 *  to reasonable ranges.
                 */
                if (ambient >= -255.0f && ambient <= 255.0f) {
                    // add fixed white ambient.
                    lb[0] += ambient;
                    lb[1] += ambient;
                    lb[2] += ambient;
                }
                if (lightscale > 0.0f) {
                    // apply lightscale, scale down or up
                    lb[0] *= lightscale;
                    lb[1] *= lightscale;
                    lb[2] *= lightscale;
                }
                // negative values not allowed
                lb[0] = (lb[0] < 0.0f) ? 0.0f : lb[0];
                lb[1] = (lb[1] < 0.0f) ? 0.0f : lb[1];
                lb[2] = (lb[2] < 0.0f) ? 0.0f : lb[2];

                /*			qprintf("{%f %f %f}:",lb[0],lb[1],lb[2]);*/

                // determine max of R,G,B
                max   = lb[0] > lb[1] ? lb[0] : lb[1];
                max   = max > lb[2] ? max : lb[2];

                if (max < 1.0f)
                    max = 1.0f;

                // note that maxlight based scaling is per-sample based on
                //  highest value of R, G, and B
                // adjust for -maxlight option
                newmax = max;
                if (max > maxlight) {
                    newmax = maxlight;
                    newmax /= max; // scaling factor 0.0..1.0
                    // scale into 0.0..maxlight range
                    lb[0] *= newmax;
                    lb[1] *= newmax;
                    lb[2] *= newmax;
                }

                // and output to 8:8:8 RGB
                *dest++ = (uint8_t)(lb[0] + 0.5);
                *dest++ = (uint8_t)(lb[1] + 0.5);
                *dest++ = (uint8_t)(lb[2] + 0.5);
            }
        }
    } else // ibsp
    {
        dface_t *f;
        f = &dfaces[facenum];

        if (texinfo[f->texinfo].flags & (SURF_WARP | SURF_SKY))
            return; // non-lit texture

        f->lightofs  = i;
        f->styles[0] = 0;
        f->styles[1] = f->styles[2] = f->styles[3] = 0xff;

        //
        // set up the triangulation
        //
        if (numbounce > 0) {
            ClearBounds(facemins, facemaxs);
            for (i = 0; i < f->numedges; i++) {
                int32_t ednum;

                ednum = dsurfedges[f->firstedge + i];
                if (ednum >= 0)
                    AddPointToBounds(dvertexes[dedges[ednum].v[0]].point,
                                     facemins, facemaxs);
                else
                    AddPointToBounds(dvertexes[dedges[-ednum].v[1]].point,
                                     facemins, facemaxs);
            }

            trian = AllocTriangulation(&dplanes[f->planenum]);

            // for all faces on the plane, add the nearby patches
            // to the triangulation
            for (pfacenum = planelinks[f->side][f->planenum]; pfacenum; pfacenum = facelinks[pfacenum]) {
                for (patch = face_patches[pfacenum]; patch; patch = patch->next) {
                    for (i = 0; i < 3; i++) {
                        if (facemins[i] - patch->origin[i] > subdiv * 2)
                            break;
                        if (patch->origin[i] - facemaxs[i] > subdiv * 2)
                            break;
                    }
                    if (i != 3)
                        continue; // not needed for this face
                    AddPointToTriangulation(patch, trian);
                }
            }
            for (i = 0; i < trian->numpoints; i++)
                memset(trian->edgematrix[i], 0, trian->numpoints * sizeof(trian->edgematrix[0][0]));
            TriangulatePoints(trian);
        }

        //
        // sample the triangulation
        //

        dest = &lightdata_ptr[f->lightofs];

        if (fl->numstyles > MAXLIGHTMAPS) {
            fl->numstyles = MAXLIGHTMAPS;
            //	printf ("face with too many lightstyles: (%f %f %f)\n",
            //		face_patches[facenum]->origin[0],
            //		face_patches[facenum]->origin[1],
            //		face_patches[facenum]->origin[2]
            //		);
        }
        for (st = 0; st < fl->numstyles; st++) {
            last_valid    = NULL;
            f->styles[st] = fl->stylenums[st];

            for (j = 0; j < fl->numsamples; j++) {
                VectorCopy((fl->samples[st] + j * 3), lb);
                if (numbounce > 0 && st == 0) {
                    vec3_t add;

                    SampleTriangulation(fl->origins + j * 3, trian, &last_valid, add);
                    VectorAdd(lb, add, lb);
                }

                /*
                 * to allow experimenting, ambient and lightscale are not limited
                 *  to reasonable ranges.
                 */
                if (ambient >= -255.0f && ambient <= 255.0f) {
                    // add fixed white ambient.
                    lb[0] += ambient;
                    lb[1] += ambient;
                    lb[2] += ambient;
                }
                if (lightscale > 0.0f) {
                    // apply lightscale, scale down or up
                    lb[0] *= lightscale;
                    lb[1] *= lightscale;
                    lb[2] *= lightscale;
                }
                // negative values not allowed
                lb[0] = (lb[0] < 0.0f) ? 0.0f : lb[0];
                lb[1] = (lb[1] < 0.0f) ? 0.0f : lb[1];
                lb[2] = (lb[2] < 0.0f) ? 0.0f : lb[2];

                /*			qprintf("{%f %f %f}:",lb[0],lb[1],lb[2]);*/

                // determine max of R,G,B
                max   = lb[0] > lb[1] ? lb[0] : lb[1];
                max   = max > lb[2] ? max : lb[2];

                if (max < 1.0f)
                    max = 1.0f;

                // note that maxlight based scaling is per-sample based on
                //  highest value of R, G, and B
                // adjust for -maxlight option
                newmax = max;
                if (max > maxlight) {
                    newmax = maxlight;
                    newmax /= max; // scaling factor 0.0..1.0
                    // scale into 0.0..maxlight range
                    lb[0] *= newmax;
                    lb[1] *= newmax;
                    lb[2] *= newmax;
                }

                // and output to 8:8:8 RGB
                *dest++ = (uint8_t)(lb[0] + 0.5);
                *dest++ = (uint8_t)(lb[1] + 0.5);
                *dest++ = (uint8_t)(lb[2] + 0.5);
            }
        }
    }

    if (numbounce > 0)
        FreeTriangulation(trian);
}
