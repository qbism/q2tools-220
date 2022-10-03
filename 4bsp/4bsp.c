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

/*
============
main
============
*/
int32_t main(int32_t argc, char **argv) {
    int32_t i;
    char path[2053]     = "";
    char tgamedir[1024] = "", tbasedir[1024] = "", tmoddir[1024] = "";

    printf("\n\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< 4bsp >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    printf("BSP compiler build " __DATE__ "\n");

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-noorigfix")) {
            printf("origfix = false\n");
            origfix = false;
        } else if (!strcmp(argv[i], "-v")) {
            printf("verbose = true\n");
            verbose = true;
        } else if (!strcmp(argv[i], "-help")) {
            printf("4bsp supporting v38 and v220 map formats plus QBSP extended limits.\n"
                   "Usage: 4bsp [options] [mapname]\n\n"
                   "    -chop #: Subdivide size.\n"
                   "        Default: 240  Range: 32-1024\n"
                   "    -choplight #: Subdivide size for surface lights.\n"
                   "        Default: 240  Range: 32-1024\n"
                   "    -largebounds: Increase max map size for supporting engines.\n"
                   "    -micro #: Minimum microbrush size. Default: 0.02\n"
                   "        Suggested range: 0.02 - 1.0\n"
                   "    -nosubdiv: Disable subdivision.\n"
                   "    -qbsp: Greatly expanded map and entity limits for supporting engines.\n"
                   "    -moddir [path]: Set a mod directory. Default is parent dir of map file.\n"
                   "    -basedir [path]: Set the directory for assets not in moddir. Default is moddir.\n"
                   "    -gamedir [path]: Set game directory, the folder with game executable.\n"
                   //"    -threads #: number of CPU threads to use\n"
                   "Debugging tools:\n"
                   "    -block # #: Division tree block size, square\n"
                   "    -blocks # # # #: Div tree block size, rectangular\n"
                   "    -blocksize: map cube size for processing. Default: 1024\n"
                   "    -fulldetail: Change most brushes to detail.\n"
                   "    -leaktest: Perform leak test only.\n"
                   "    -nocsg: No constructive solid geometry.\n"
                   "    -nodetail: No detail brushes.\n"
                   "    -nomerge: Don't merge visible faces per node.\n"
                   "    -noorigfix: Disable texture fix for origin offsets.\n"
                   "    -noprune: Disable node pruning.\n"
                   "    -noshare: Don't look for shared edges on save.\n"
                   "    -noskipfix: Do not automatically set skip contents to zero.\n"
                   "    -notjunc: Disable edge cleanup.\n"
                   "    -nowater: Ignore warp surfaces.\n"
                   "    -noweld: Disable vertex welding.\n"
                   "    -onlyents: Grab the entites and resave.\n"
                   "    -v: Display more verbose output.\n"
                   "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< 4bsp HELP >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n\n");

            exit(1);
        }
        /*
                if (!strcmp(argv[i],"-threads"))
                {
                    numthreads = atoi (argv[i+1]);
                    i++;
                }
        */
        else if (!strcmp(argv[i], "-noweld")) {
            printf("noweld = true\n");
            noweld = true;
        } else if (!strcmp(argv[i], "-nocsg")) {
            printf("nocsg = true\n");
            nocsg = true;
        } else if (!strcmp(argv[i], "-noshare")) {
            printf("noshare = true\n");
            noshare = true;
        } else if (!strcmp(argv[i], "-notjunc")) {
            printf("notjunc = true\n");
            notjunc = true;
        } else if (!strcmp(argv[i], "-nowater")) {
            printf("nowater = true\n");
            nowater = true;
        } else if (!strcmp(argv[i], "-noprune")) {
            printf("noprune = true\n");
            noprune = true;
        } else if (!strcmp(argv[i], "-nomerge")) {
            printf("nomerge = true\n");
            nomerge = true;
        } else if (!strcmp(argv[i], "-nosubdiv")) {
            printf("nosubdiv = true\n");
            nosubdiv = true;
        } else if (!strcmp(argv[i], "-nodetail")) {
            printf("nodetail = true\n");
            nodetail = true;
        } else if (!strcmp(argv[i], "-fulldetail")) {
            printf("fulldetail = true\n");
            fulldetail = true;
        } else if (!strcmp(argv[i], "-onlyents")) {
            printf("onlyents = true\n");
            onlyents = true;
        } else if (!strcmp(argv[i], "-micro")) {
            microvolume = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-leaktest")) {
            printf("leaktest = true\n");
            leaktest = true;
        }

        // qb: qbsp
        else if (!strcmp(argv[i], "-qbsp")) {
            printf("use_qbsp = true\n");
            use_qbsp     = true;
            max_entities = MAX_MAP_ENTITIES_QBSP;
            max_bounds   = MAX_MAP_SIZE;
            block_size   = MAX_BLOCK_SIZE; // qb: otherwise limits map range
        } else if (!strcmp(argv[i], "-noskipfix")) {
            printf("noskipfix = true\n");
            noskipfix = true;

        }

        // qb: from kmqbsp3- Knightmare added
        else if (!strcmp(argv[i], "-largebounds") || !strcmp(argv[i], "-lb")) {
            if (use_qbsp) {
                printf("[-largebounds is not required with -qbsp]\n");
            } else {
                max_bounds = MAX_MAP_SIZE;
                block_size = MAX_BLOCK_SIZE; // qb: otherwise limits map range
                printf("largebounds: using max bound size of %i\n", MAX_MAP_SIZE);
            }
        }

        // qb:  set gamedir, moddir, and basedir
        else if (!strcmp(argv[i], "-gamedir")) {
            strcpy(tgamedir, argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-moddir")) {
            strcpy(tmoddir, argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-basedir")) {
            strcpy(tbasedir, argv[i + 1]);
            i++;
        }

        else if ((!strcmp(argv[i], "-chop")) || (!strcmp(argv[i], "-subdiv"))) {
            subdivide_size = atof(argv[i + 1]);
            if (subdivide_size < 32) {
                subdivide_size = 32;
                printf("subdivide_size set to minimum size: 32\n");
            }
            if (subdivide_size > 1024) {
                subdivide_size = 1024;
                printf("subdivide_size set to maximum size: 1024\n");
            }
            printf("subdivide_size = %f\n", subdivide_size);
            i++;
        } else if ((!strcmp(argv[i], "-choplight")) || (!strcmp(argv[i], "-choplights")) || (!strcmp(argv[i], "-subdivlight"))) // qb: chop surf lights independently
        {
            sublight_size = atof(argv[i + 1]);
            if (sublight_size < 32) {
                sublight_size = 32;
                printf("sublight_size set to minimum size: 32\n");
            }
            if (sublight_size > 1024) {
                sublight_size = 1024;
                printf("sublight_size set to maximum size: 1024\n");
            }
            printf("sublight_size = %f\n", sublight_size);
            i++;
        } else if (!strcmp(argv[i], "-blocksize")) {
            block_size = atof(argv[i + 1]);
            if (block_size < 128) {
                block_size = 128;
                printf("block_size set to minimum size: 128\n");
            }
            if (block_size > MAX_BLOCK_SIZE) {
                block_size = MAX_BLOCK_SIZE;
                printf("block_size set to minimum size: MAX_BLOCK_SIZE\n");
            }
            printf("blocksize: %i\n", block_size);
            i++;
        } else if (!strcmp(argv[i], "-block")) {
            block_xl = block_yl = atoi(argv[i + 1]); // qb: fixed... has it always been wrong? was xl = xh and yl = yh
            block_xh = block_yh = atoi(argv[i + 2]);
            printf("block: %i,%i\n", block_xl, block_xh);
            i += 2;
        } else if (!strcmp(argv[i], "-blocks")) {
            block_xl = atoi(argv[i + 1]);
            block_yl = atoi(argv[i + 2]);
            block_xh = atoi(argv[i + 3]);
            block_yh = atoi(argv[i + 4]);
            printf("blocks: %i,%i to %i,%i\n",
                   block_xl, block_yl, block_xh, block_yh);
            i += 4;
        } else
            break;
    }

    if (i != argc - 1) {
        printf("Supporting v38 and v220 map formats plus QBSP extended limits.\n"
               "Usage: 4bsp [options] [mapname]\n"
               "    -chop #                  -choplight #         -help\n"
               "    -largebounds             -micro #             -nosubdiv\n"
               "    -qbsp                    -gamedir             -basedir\n"
               "Debugging tools:             -block # #           -blocks # # # #\n"
               "    -blocksize #             -fulldetail          -leaktest\n"
               "    -nocsg                   -nodetail            -nomerge\n"
               "    -noorigfix               -noprune             -noshare\n"
               "    -noskipfix               -notjunc             -nowater\n"
               "    -noweld                  -onlyents            -v (verbose)\n\n");

        exit(1);
    }

    ThreadSetDefault();

    // qb: below is from original source release.  On Windows, multi threads cause false leak errors.
    numthreads = 1; // multiple threads aren't helping...

    SetQdirFromPath(argv[i]);

    if (strcmp(tmoddir, "")) {
        strcpy(moddir, tmoddir);
        Q_pathslash(moddir);
        strcpy(basedir, moddir);
    }
    if (strcmp(tbasedir, "")) {
        strcpy(basedir, tbasedir);
        Q_pathslash(basedir);
        if (!strcmp(tmoddir, ""))
            strcpy(moddir, basedir);
    }
    if (strcmp(tgamedir, "")) {
        strcpy(gamedir, tgamedir);
        Q_pathslash(gamedir);
    }
   printf("microvolume = %f\n\n", microvolume);

    // qb: display dirs
    printf("moddir = %s\n", moddir);
    printf("basedir = %s\n", basedir);
    printf("gamedir = %s\n", gamedir);
 
    strcpy(source, ExpandArg(argv[i]));
    StripExtension(source);

    // delete portal and line files
    sprintf(path, "%s.prt", source);
    remove(path);
    sprintf(path, "%s.pts", source);
    remove(path);

    strcpy(name, ExpandArg(argv[i]));
    DefaultExtension(name, ".map"); // might be .reg

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

    return 0;
}
