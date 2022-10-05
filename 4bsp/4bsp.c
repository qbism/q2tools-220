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

#include "qbsp.h"

extern float subdivide_size;
extern float sublight_size;

char source[1024];
char name[1024];

vec_t microvolume        = 0.02f; // jit - was 1.0, but this messes up small brushes
qboolean noprune         = false;
qboolean glview          = false;
qboolean nodetail        = false;
qboolean fulldetail      = false;
qboolean onlyents        = false;
qboolean nomerge         = false;
qboolean nowater         = false;
qboolean nocsg           = false;
qboolean noweld          = false;
qboolean noshare         = false;
qboolean nosubdiv        = false;
qboolean notjunc         = false;
qboolean leaktest        = false;
qboolean badnormal_check = false;
qboolean origfix         = true; // default to true

int32_t block_xl = -8, block_xh = 7, block_yl = -8, block_yh = 7;

int32_t entity_num;

int32_t max_entities = MAX_MAP_ENTITIES; // qb: from kmqbsp3- Knightmare- adjustable entity limit
int32_t max_bounds   = DEFAULT_MAP_SIZE; // Knightmare- adjustable max bounds
int32_t block_size   = 1024;             // Knightmare- adjustable block size

node_t *block_nodes[10][10];

/*
============
BlockTree

============
*/
node_t *BlockTree(int32_t xl, int32_t yl, int32_t xh, int32_t yh) {
    node_t *node;
    vec3_t normal;
    vec_t dist;
    int32_t mid;

    if (xl == xh && yl == yh) {
        node = block_nodes[xl + 5][yl + 5];
        if (!node) {
            // return an empty leaf
            node           = AllocNode();
            node->planenum = PLANENUM_LEAF;
            node->contents = 0; // CONTENTS_SOLID;
            return node;
        }
        return node;
    }

    // create a seperator along the largest axis
    node = AllocNode();

    if (xh - xl > yh - yl) {
        // split x axis
        mid               = xl + (xh - xl) / 2 + 1;
        normal[0]         = 1;
        normal[1]         = 0;
        normal[2]         = 0;
        dist              = mid * block_size;
        node->planenum    = FindFloatPlane(normal, dist, 0);
        node->children[0] = BlockTree(mid, yl, xh, yh);
        node->children[1] = BlockTree(xl, yl, mid - 1, yh);
    } else {
        mid               = yl + (yh - yl) / 2 + 1;
        normal[0]         = 0;
        normal[1]         = 1;
        normal[2]         = 0;
        dist              = mid * block_size;
        node->planenum    = FindFloatPlane(normal, dist, 0);
        node->children[0] = BlockTree(xl, mid, xh, yh);
        node->children[1] = BlockTree(xl, yl, xh, mid - 1);
    }

    return node;
}

/*
============
ProcessBlock_Thread

============
*/
int32_t brush_start, brush_end;
void ProcessBlock_Thread(int32_t blocknum) {
    int32_t xblock, yblock;
    vec3_t mins, maxs;
    bspbrush_t *brushes;
    tree_t *tree;
    node_t *node;

    yblock = block_yl + blocknum / (block_xh - block_xl + 1);
    xblock = block_xl + blocknum % (block_xh - block_xl + 1);

    qprintf("############### block %2i,%2i ###############\n", xblock, yblock);

    mins[0] = xblock * block_size;
    mins[1] = yblock * block_size;
    mins[2] = -max_bounds; // was -4096
    maxs[0] = (xblock + 1) * block_size;
    maxs[1] = (yblock + 1) * block_size;
    maxs[2] = max_bounds; // was 4096

    // the makelist and chopbrushes could be cached between the passes...
    brushes = MakeBspBrushList(brush_start, brush_end, mins, maxs);
    if (!brushes) {
        node                                = AllocNode();
        node->planenum                      = PLANENUM_LEAF;
        node->contents                      = CONTENTS_SOLID;
        block_nodes[xblock + 5][yblock + 5] = node;
        return;
    }

    if (!nocsg)
        brushes = ChopBrushes(brushes);

    tree                                = BrushBSP(brushes, mins, maxs);

    block_nodes[xblock + 5][yblock + 5] = tree->headnode;
}

/*
============
ProcessWorldModel

============
*/
void ProcessWorldModel(void) {
    entity_t *e;
    tree_t *tree;
    qboolean leaked;
    qboolean optimize;

    e           = &entities[entity_num];

    brush_start = e->firstbrush;
    brush_end   = brush_start + e->numbrushes;
    leaked      = false;

    //
    // perform per-block operations
    //
    if (block_xh * block_size > map_maxs[0])
        block_xh = floor(map_maxs[0] / block_size);
    if ((block_xl + 1) * block_size < map_mins[0])
        block_xl = floor(map_mins[0] / block_size);
    if (block_yh * block_size > map_maxs[1])
        block_yh = floor(map_maxs[1] / block_size);
    if ((block_yl + 1) * block_size < map_mins[1])
        block_yl = floor(map_mins[1] / block_size);

    if (block_xl < -4)
        block_xl = -4;
    if (block_yl < -4)
        block_yl = -4;
    if (block_xh > 3)
        block_xh = 3;
    if (block_yh > 3)
        block_yh = 3;

    for (optimize = false; optimize <= true; optimize++) {
        qprintf("--------------------------------------------\n");

        RunThreadsOnIndividual((block_xh - block_xl + 1) * (block_yh - block_yl + 1),
                               !verbose, ProcessBlock_Thread);

        //
        // build the division tree
        // oversizing the blocks guarantees that all the boundaries
        // will also get nodes.
        //

        qprintf("--------------------------------------------\n");

        tree           = AllocTree();
        tree->headnode = BlockTree(block_xl - 1, block_yl - 1, block_xh + 1, block_yh + 1);

        tree->mins[0]  = (block_xl)*block_size;
        tree->mins[1]  = (block_yl)*block_size;
        tree->mins[2]  = map_mins[2] - 8;

        tree->maxs[0]  = (block_xh + 1) * block_size;
        tree->maxs[1]  = (block_yh + 1) * block_size;
        tree->maxs[2]  = map_maxs[2] + 8;

        //
        // perform the global operations
        //
        MakeTreePortals(tree);

        if (FloodEntities(tree))
            FillOutside(tree->headnode);
        else {
            printf("**** leaked ****\n");
            leaked = true;
            LeakFile(tree);
            if (leaktest) {
                printf("--- MAP LEAKED ---\n");
                exit(0);
            }
        }

        MarkVisibleSides(tree, brush_start, brush_end);
        if (leaked)
            break;
        if (!optimize) {
            FreeTree(tree);
        }
    }

    FloodAreas(tree);
    MakeFaces(tree->headnode);
    FixTjuncs(tree->headnode);

    if (!noprune)
        PruneNodes(tree->headnode);

    WriteBSP(tree->headnode);

    if (!leaked)
        WritePortalFile(tree);

    FreeTree(tree);
}

/*
============
ProcessSubModel

============
*/
void ProcessSubModel(void) {
    entity_t *e;
    int32_t start, end;
    tree_t *tree;
    bspbrush_t *list;
    vec3_t mins, maxs;

    e       = &entities[entity_num];

    start   = e->firstbrush;
    end     = start + e->numbrushes;

    mins[0] = mins[1] = mins[2] = -max_bounds;
    maxs[0] = maxs[1] = maxs[2] = max_bounds;
    list                        = MakeBspBrushList(start, end, mins, maxs);
    if (!nocsg)
        list = ChopBrushes(list);
    tree = BrushBSP(list, mins, maxs);
    MakeTreePortals(tree);
    MarkVisibleSides(tree, start, end);
    MakeFaces(tree->headnode);
    FixTjuncs(tree->headnode);
    WriteBSP(tree->headnode);
    FreeTree(tree);
}

/*
============
ProcessModels
============
*/
void ProcessModels(void) {
    BeginBSPFile();

    for (entity_num = 0; entity_num < num_entities; entity_num++) {
        if (!entities[entity_num].numbrushes)
            continue;

        qprintf("############### model %i ###############\n", nummodels);
        BeginModel();
        if (entity_num == 0)
            ProcessWorldModel();
        else
            ProcessSubModel();
        EndModel();
    }

    EndBSPFile();
}

void BSP_ProcessArgument(const char * arg) {
    char path[2053];

    strcpy(source, ExpandArg(arg));
    StripExtension(source);

    // delete portal and line files
    sprintf(path, "%s.prt", source);
    remove(path);
    sprintf(path, "%s.pts", source);
    remove(path);

    strcpy(name, ExpandArg(arg));
    DefaultExtension(name, ".map"); // might be .reg

    InitBSPFile();

    //
    // if onlyents, just grab the entites and resave
    //
    if (onlyents) {
        char out[2053];

        sprintf(out, "%s.bsp", source);
        LoadBSPFile(out);
        if (use_qbsp)
            printf("use_qbsp = true\n");

        num_entities = 0;

        LoadMapFile(name);
        SetModelNumbers();
        SetLightStyles();

        UnparseEntities();

        WriteBSPFile(out);
    } else {
        extern void ProcessModels();

        //
        // start from scratch
        //
        LoadMapFile(name);
        SetModelNumbers();
        SetLightStyles();

        ProcessModels();
    }

    PrintBSPFileSizes();
    printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< END 4bsp >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n\n");
}