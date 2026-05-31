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

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
===========================================================================
*/

#include "qrad.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* based on ericw-tools implementation of lightgrid. 
Note on support of multiple styles:
Lightgrid_samples_t struct supports 4 styles, but code currently only writes Style 0.
If a player walks near a flickering light, their model won't flicker.
Baking the other styles into the lump requires an engine that supports multi-style grids.
*/

/* Range check: skip samples further than this from any surface to save time. 
   1024 is the standard used by ericw-tools and ZHLT. */
#define LG_MAX_DIST 1024.0f

static lightgrid_t octree_grid;
static uint8_t *grid_buffer = NULL;

/* Lightgrids now only support the octree format and 
   are only generated for QBSP maps. */

extern char source[1024];

static vec3_t axial_dirs[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
};

static FILE *lg_fp;

void CalcLightgridAtPoint(vec3_t point, lightgrid_samples_t *out) {
    if (!out) return;

    memset(out, 0, sizeof(*out));

    /* Standard Lightgrid expects a color for each of the 6 faces of a cube
       to handle dynamic lighting correctly from all angles. */
    uint8_t pvs[(MAX_MAP_LEAFS_QBSP + 7) / 8];
    if (!PvsForOrigin(point, pvs)) {
        out->occluded = true;
        return;
    }

    int nodenum = PointInNodenum(point);
    int mapsize = sizeof(vec3_t);

    for (int i = 0; i < 6; i++) {
        float *styletable[MAX_LSTYLES];
        memset(styletable, 0, sizeof(styletable));
        bool sun_main_once = false, sun_ambient_once = false;

        /* Sample light coming from this axial direction. */
        GatherSampleLight(point, axial_dirs[i], styletable, 0, mapsize, 1.0f,
                          &sun_main_once, &sun_ambient_once, pvs, nodenum, true);

        /* Fold results into the compact `lightgrid_samples_t` structure. */
        for (int s = 0; s < MAX_LSTYLES; s++) {
            if (!styletable[s]) continue;

            int style_idx = -1;
            for (int k = 0; k < 4; k++) {
                if (out->samples_by_style[k].used && out->samples_by_style[k].style == s) {
                    style_idx = k;
                    break;
                }
                if (!out->samples_by_style[k].used) {
                    style_idx = k;
                    out->samples_by_style[k].used = true;
                    out->samples_by_style[k].style = s;
                    break;
                }
            }

            if (style_idx != -1) {
                VectorCopy(styletable[s], out->samples_by_style[style_idx].colors[i]);
                VectorAdd(out->samples_by_style[style_idx].undirectional_color, styletable[s], out->samples_by_style[style_idx].undirectional_color);
            }
            free(styletable[s]);
        }
    }

    /* Average the undirectional color across all sampled directions. */
    for (int k = 0; k < 4; k++) {
        if (out->samples_by_style[k].used) {
            VectorScale(out->samples_by_style[k].undirectional_color, 1.0f / 6.0f, out->samples_by_style[k].undirectional_color);
        }
    }
}

/* Helper to check if a point is within range of any face in its leaf. */
static bool IsPointNearGeometry(const vec3_t pos, int leafnum, float range) {
    int i;
    float dist;
    vec_t *v;

    if (use_qbsp) {
        dleaf_tx *leaf = &dleafsX[leafnum];
        for (i = 0; i < leaf->numleaffaces; i++) {
            int facenum = dleaffacesX[leaf->firstleafface + i];
            dface_tx *f = &dfacesX[facenum];
            dplane_t *p = &dplanes[f->planenum];
            
            dist = fabs(DotProduct(pos, p->normal) - p->dist);
            if (dist < range) return true;
        }
    } else {
        dleaf_t *leaf = &dleafs[leafnum];
        for (i = 0; i < leaf->numleaffaces; i++) {
            int facenum = dleaffaces[leaf->firstleafface + i];
            dface_t *f = &dfaces[facenum];
            dplane_t *p = &dplanes[f->planenum];

            dist = fabs(DotProduct(pos, p->normal) - p->dist);
            if (dist < range) return true;
        }
    }

    /* Note: A more thorough check would look at neighboring leaves via PVS, 
       but checking the current leaf is a very fast first-pass optimization. */
    return false;
}

/* Matches logic in lightmap.c:FinalLightFace to ensure brightness consistency between 
   surfaces and the lightgrid. Applies global ambient, lightscale, and hue-preserving clamping. */
static void LightGrid_FinalColor(const vec3_t in, uint8_t *out) {
    vec3_t lb;
    float max, newmax;

    VectorCopy(in, lb);

    lb[0] += ambient;
    lb[1] += ambient;
    lb[2] += ambient;

    lb[0] *= lightscale;
    lb[1] *= lightscale;
    lb[2] *= lightscale;

    lb[0] = (lb[0] < 0.0f) ? 0.0f : lb[0];
    lb[1] = (lb[1] < 0.0f) ? 0.0f : lb[1];
    lb[2] = (lb[2] < 0.0f) ? 0.0f : lb[2];

    max = lb[0] > lb[1] ? lb[0] : lb[1];
    max = max > lb[2] ? max : lb[2];
    if (max < 1.0f) max = 1.0f;

    if (max > maxlight) {
        newmax = maxlight / max;
        lb[0] *= newmax;
        lb[1] *= newmax;
        lb[2] *= newmax;
    }

    out[0] = (uint8_t)(lb[0] + 0.5f);
    out[1] = (uint8_t)(lb[1] + 0.5f);
    out[2] = (uint8_t)(lb[2] + 0.5f);
}

static void LightGrid_SetOccluded(uint8_t *ptr) {
    // Flag as occluded using 255 style count as expected by engine loader
    *ptr = 255;
    // Clear color data for consistency during potential blending passes
    memset(ptr + 1, 0, 6);
}

static void LightGrid_Thread(int chunk) {
    int x, y, z;
    int total_xy = octree_grid.size[0] * octree_grid.size[1];
    
    z = chunk / total_xy;
    int remainder = chunk % total_xy;
    y = remainder / octree_grid.size[0];
    x = remainder % octree_grid.size[0];

    /*
       Octree: store the raw samples in an intermediate 
       buffer and build the tree structure after all threads finish.
    */
    uint8_t *sample_out = grid_buffer + chunk * 7;

    vec3_t pos;
    pos[0] = octree_grid.mins[0] + x * octree_grid.scale[0];
    pos[1] = octree_grid.mins[1] + y * octree_grid.scale[1];
    pos[2] = octree_grid.mins[2] + z * octree_grid.scale[2];

    int leafnum = PointInLeafnum(pos);
    int contents = use_qbsp ? dleafsX[leafnum].contents : dleafs[leafnum].contents;

    /* Skip samples inside solid. */
    if (contents & CONTENTS_SOLID) {
        LightGrid_SetOccluded(sample_out);
        return;
    }

    /* Range check: skip samples that are too far from any geometry. */
    /* Note: In Quake 2, leaf-based face checks can be overly restrictive in large rooms.
       We will sample all non-solid areas to ensure complete coverage for now. */
    // if (!IsPointNearGeometry(pos, leafnum, LG_MAX_DIST)) {
    //     LightGrid_SetOccluded(sample_out);
    //     return;
    // }

    lightgrid_samples_t res;
    CalcLightgridAtPoint(pos, &res);

    if (lg_debug) {
        ThreadLock();
        if (lg_fp) fprintf(lg_fp, "%f %f %f\n", pos[0], pos[1], pos[2]);
        ThreadUnlock();
    }

    /* Bake Style 0 directly into the BSP lump. */
    lightgrid_raw_sample_t *s0 = &res.samples_by_style[0];

    *sample_out = 2; // Style count: Ambient + Directional
    uint8_t *ptr = sample_out + 1;

    /* 6-byte format: 3 bytes Ambient RGB + 3 bytes Directional RGB.
       Ambient is the average/undirectional color.
       Directional is the color of the brightest sampled axial direction */

    LightGrid_FinalColor(s0->undirectional_color, ptr);
    ptr += 3;

    vec3_t max_color = {0, 0, 0};
    float max_intensity = -1.0f;
    for (int d = 0; d < 6; d++) {
        float intensity = s0->colors[d][0] + s0->colors[d][1] + s0->colors[d][2];
        if (intensity > max_intensity) {
            max_intensity = intensity;
            VectorCopy(s0->colors[d], max_color);
        }
    }

    vec3_t dir_color;
    VectorSubtract(max_color, s0->undirectional_color, dir_color);
    for (int i = 0; i < 3; i++) if (dir_color[i] < 0) dir_color[i] = 0;

    LightGrid_FinalColor(dir_color, ptr);
}

/* Blends adjacent lightgrid samples to smooth out harsh transitions.
   Uses a weighted average: current sample 1.0, each neighbor w */
static void LightGrid_Blend(void) {
    size_t total_samples = (size_t)octree_grid.size[0] * octree_grid.size[1] * octree_grid.size[2];
    uint8_t *blended = malloc(total_samples * 7);
    if (!blended) return;

    printf("Smoothing lightgrid samples...\n");

    int size_x = octree_grid.size[0];
    int size_y = octree_grid.size[1];
    int size_z = octree_grid.size[2];

    for (int z = 0; z < size_z; z++) {
        for (int y = 0; y < size_y; y++) {
            for (int x = 0; x < size_x; x++) {
                int idx = (z * size_y * size_x) + (y * size_x) + x;
                uint8_t *src = grid_buffer + idx * 7;
                
                // Skip occluded samples - they don't contribute to neighbors and shouldn't be smoothed
                if (src[0] == 255) {
                    memcpy(blended + idx * 7, src, 7);
                    continue;
                }

                float amb[3] = { (float)src[1], (float)src[2], (float)src[3] };
                float dir[3] = { (float)src[4], (float)src[5], (float)src[6] };
                float total_weight = 1.0f;

                int dx[] = {-1, 1, 0, 0, 0, 0};
                int dy[] = {0, 0, -1, 1, 0, 0};
                int dz[] = {0, 0, 0, 0, -1, 1};

                for (int i = 0; i < 6; i++) {
                    int nx = x + dx[i];
                    int ny = y + dy[i];
                    int nz = z + dz[i];

                    if (nx >= 0 && nx < size_x && ny >= 0 && ny < size_y && nz >= 0 && nz < size_z) {
                        int nidx = (nz * size_y * size_x) + (ny * size_x) + nx;
                        uint8_t *nsrc = grid_buffer + nidx * 6;
                        
                        float w = 0.75f;
                        amb[0] += (float)nsrc[0] * w;
                        amb[1] += (float)nsrc[1] * w;
                        amb[2] += (float)nsrc[2] * w;
                        dir[0] += (float)nsrc[3] * w;
                        dir[1] += (float)nsrc[4] * w;
                        dir[2] += (float)nsrc[5] * w;
                        total_weight += w;
                    }
                }

                uint8_t *dst = blended + idx * 6;
                dst[0] = (uint8_t)(amb[0] / total_weight + 0.5f);
                dst[1] = (uint8_t)(amb[1] / total_weight + 0.5f);
                dst[2] = (uint8_t)(amb[2] / total_weight + 0.5f);
                dst[3] = (uint8_t)(dir[0] / total_weight + 0.5f);
                dst[4] = (uint8_t)(dir[1] / total_weight + 0.5f);
                dst[5] = (uint8_t)(dir[2] / total_weight + 0.5f);
            }
        }
    }

    free(grid_buffer);
    grid_buffer = blended;
}

void LightGrid_Process(void) {
    printf("--- LightGrid_Process ---\n");

    if (!use_qbsp) {
        printf("LightGrid_Process: Lightgrids are only supported for QBSP format maps, skipping.\n");
        return;
    }

    if (nummodels <= 0) {
        printf("LightGrid_Process: No models found, skipping.\n");
        return;
    }

    // Calculate quantization bounds based on world model (dmodels[0]).
    VectorCopy(dmodels[0].mins, octree_grid.mins);
    octree_grid.mins[0] = lg_step[0] * floor(octree_grid.mins[0] / lg_step[0]);
    octree_grid.mins[1] = lg_step[1] * floor(octree_grid.mins[1] / lg_step[1]);
    octree_grid.mins[2] = lg_step[2] * floor(octree_grid.mins[2] / lg_step[2]);

    octree_grid.size[0] = (int)ceil((dmodels[0].maxs[0] - octree_grid.mins[0]) / lg_step[0]) + 1;
    octree_grid.size[1] = (int)ceil((dmodels[0].maxs[1] - octree_grid.mins[1]) / lg_step[1]) + 1;
    octree_grid.size[2] = (int)ceil((dmodels[0].maxs[2] - octree_grid.mins[2]) / lg_step[2]) + 1;

    octree_grid.scale[0] = lg_step[0];
    octree_grid.scale[1] = lg_step[1];
    octree_grid.scale[2] = lg_step[2];

    size_t total_samples = (size_t)octree_grid.size[0] * octree_grid.size[1] * octree_grid.size[2];
    size_t expected_size = 45 + 4 + 24 + (total_samples * 7); // Header(45) + NumLeafs(4) + 1 Leaf Header(24) + Samples(N*7)

    if (expected_size > MAX_MAP_LIGHTGRID_QBSP) {
        Error("LightGrid_Process: Grid exceeds MAX_MAP_LIGHTGRID_QBSP (%u > %u bytes).", 
              (unsigned int)expected_size, (unsigned int)MAX_MAP_LIGHTGRID_QBSP);
    }

    printf("Grid Size: %d x %d x %d (%d samples)\n", octree_grid.size[0], octree_grid.size[1], octree_grid.size[2], (int)total_samples);

    grid_buffer = malloc(total_samples * 7);
    if (!grid_buffer) {
        Error("LightGrid_Process: Failed to allocate intermediate grid buffer.");
    }

    if (lg_debug) {
        char debugname[1024];
        sprintf(debugname, "%s_lg.pts", source);
        lg_fp = fopen(debugname, "w");
        if (!lg_fp) {
            printf("Warning: LightGrid_Process: Couldn't open %s for writing.\n", debugname);
        }
    }

    RunThreadsOnIndividual(total_samples, true, LightGrid_Thread);

    // Smooth the results before quantization/compression.
    LightGrid_Blend();

    // Prepare octree metadata for single-leaf compatible format
    octree_grid.numleafs = 1;
    octree_grid.numnodes = 0;
    octree_grid.rootnode = 0 | (1 << 31); // FLAG_LEAF set, leaf index 0
    octree_grid.numstyles = 2; // Ambient + Directional

    uint8_t *out = lightgrid;

    // 1. Header (45 bytes)
    memcpy(out, octree_grid.scale, 12); out += 12;
    memcpy(out, octree_grid.size, 12); out += 12;
    memcpy(out, octree_grid.mins, 12); out += 12;
    *out = (uint8_t)octree_grid.numstyles; out += 1;
    memcpy(out, &octree_grid.rootnode, 4); out += 4;
    memcpy(out, &octree_grid.numnodes, 4); out += 4;

    // 2. Nodes Array (none)

    // 3. numleafs (4 bytes)
    memcpy(out, &octree_grid.numleafs, 4); out += 4;

    // 4. Leafs and samples Block
    uint32_t zero_mins[3] = {0, 0, 0};
    memcpy(out, zero_mins, 12); out += 12;      // leaf->mins
    memcpy(out, octree_grid.size, 12); out += 12; // leaf->size

    uint8_t *src = grid_buffer;
    for (size_t i = 0; i < total_samples; i++) {
        uint8_t styles = src[0];
        *out = styles; out += 1;

        if (styles != 255) {
            // Write 6 bytes (Ambient RGB + Directional RGB)
            memcpy(out, src + 1, 6);
            out += 6;
        }

        src += 7;
    }

    lightgridsize = out - lightgrid;

    if (lg_fp) {
        fclose(lg_fp);
    }

    free(grid_buffer);
    printf("LightGrid Octree: %d bytes processed.\n", lightgridsize);
}

void FixPointAndCalcLightgrid(vec3_t point, const vec3_t normal, lightgrid_samples_t *out) {
    vec3_t pos;
    VectorCopy(point, pos);
    /* Nudge the sample slightly along the normal to avoid being exactly on a surface. */
    VectorMA(pos, sample_nudge, normal, pos);
    CalcLightgridAtPoint(pos, out);
}
