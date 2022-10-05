#include "qbsp.h"

extern char name[1024];

extern qboolean origfix;
extern qboolean nocsg;
extern qboolean onlyents;
extern qboolean leaktest;
extern int32_t block_size;
extern float subdivide_size;
extern float sublight_size;
extern int32_t block_xl;
extern int32_t block_yl;
extern int32_t block_xh;
extern int32_t block_yh;

void BSP_ProcessArgument(const char * arg) ;

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

    BSP_ProcessArgument(argv[i]);

    return 0;
}
