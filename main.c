#include "common/cmdlib.h"
#include "common/mathlib.h"
#include "common/qfiles.h"
#include "common/threads.h"

static char * help_string =
    "4bld supporting v38 and v220 map formats plus QBSP extended limits.\n"
    "Usage: 4bld [options] [mapfile | bspfile]\n\n"
    "    -moddir [path]: Set a mod directory. Default is parent dir of map file.\n"
    "    -basedir [path]: Set the directory for assets not in moddir. Default is moddir.\n"
    "    -gamedir [path]: Set game directory, the folder with game executable.\n"
    "    -threads #: number of CPU threads to use\n"
    "BSP pass:\n"
    "    -bsp: enable bsp pass, requires a .map file as input\n"
    "    -chop #: Subdivide size.\n"
    "        Default: 240  Range: 32-1024\n"
    "    -choplight #: Subdivide size for surface lights.\n"
    "        Default: 240  Range: 32-1024\n"
    "    -largebounds or -lb: Increase max map size for supporting engines.\n"
    "    -micro #: Minimum microbrush size. Default: 0.02\n"
    "        Suggested range: 0.02 - 1.0\n"
    "    -nosubdiv: Disable subdivision.\n"
    "    -qbsp: Greatly expanded map and entity limits for supporting engines.\n"
    "VIS pass:\n"
    "    -vis: enable vis pass, requires a .bsp file as input or bsp pass enabled\n"
    "    -fast: fast single vis pass"
    "RAD pass:\n"
    "    -rad: enable rad pass, requires a .bsp file as input or bsp and vis passes enabled\n"
    "    -ambient #\n"
    "    -bounce #\n"
    "    -dice\n"
    "    -direct #\n"
    "    -entity #\n"
    "    -extra\n"
    "    -maxdata #\n"
    "    -maxlight #\n"
    "    -noedgefix\n"
    "    -nudge #\n"
    "    -saturate #\n"
    "    -scale #\n"
    "    -smooth #\n"
    "    -subdiv\n"
    "    -sunradscale #\n"
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
    "    -dump\n"
    "    -noblock\n"
    "    -nopvs\n"
    "    -savetrace\n"
    "    -tmpin\n"
    "    -tmpout\n"
    ;

extern qboolean origfix;
extern qboolean noweld;
extern qboolean nocsg;
extern qboolean noshare;
extern qboolean notjunc;
extern qboolean nowater;
extern qboolean noprune;
extern qboolean nomerge;
extern qboolean nosubdiv;
extern qboolean nodetail;
extern qboolean fulldetail;
extern qboolean onlyents;
extern float microvolume;
extern qboolean leaktest;
extern qboolean use_qbsp;
extern int32_t max_entities;
extern int32_t max_bounds;
extern int32_t block_size;
extern qboolean noskipfix;
extern float subdivide_size;
extern float sublight_size;
extern int32_t block_xl;
extern int32_t block_yl;
extern int32_t block_xh;
extern int32_t block_yh;
extern char inbase[32];
extern char outbase[32];

extern qboolean fastvis;

extern qboolean nosort;
extern qboolean dumppatches;
extern int32_t numbounce;
extern qboolean extrasamples;
extern qboolean noedgefix;
extern int32_t maxdata;
extern float lightscale;
extern float sunradscale;
extern float patch_cutoff;
extern float direct_scale;
extern float entity_scale;
extern qboolean noblock;
extern float smoothing_value;
extern float sample_nudge;
extern float ambient;
extern qboolean save_trace;
extern float maxlight;
extern qboolean dicepatches;
extern float saturation;
extern qboolean nopvs;

void BSP_ProcessArgument(const char * arg);
void VIS_ProcessArgument(const char * arg);
void RAD_ProcessArgument(const char * arg);

int main(int argc, char *argv []) {
    char tgamedir[1024] = "";
    char tbasedir[1024] = "";
    char tmoddir[1024]  = "";

    qboolean do_bsp = false;
    qboolean do_vis = false;
    qboolean do_rad = false;

    ThreadSetDefault();

    int32_t i;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-bsp")) {
            do_bsp = true;
        } else if (!strcmp(argv[i], "-vis")) {
            do_vis = true;
        } else if (!strcmp(argv[i], "-rad")) {
            do_rad = true;
        } else if (!strcmp(argv[i], "-noorigfix")) {
            printf("origfix = false\n");
            origfix = false;
        } else if (!strcmp(argv[i], "-v")) {
            printf("verbose = true\n");
            verbose = true;
        } else if (!strcmp(argv[i], "-help")) {
            printf("%s\n", help_string);
            exit(1);
        } else if (!strcmp(argv[i],"-threads")) {
            numthreads = atoi(argv[i+1]);
            printf("threads = %i\n", numthreads);
            i++;
        } else if (!strcmp(argv[i], "-noweld")) {
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
        } else if (!strcmp(argv[i], "-qbsp")) {
            // qb: qbsp
            printf("use_qbsp = true\n");
            use_qbsp     = true;
            max_entities = MAX_MAP_ENTITIES_QBSP;
            max_bounds   = MAX_MAP_SIZE;
            block_size   = MAX_BLOCK_SIZE; // qb: otherwise limits map range
        } else if (!strcmp(argv[i], "-noskipfix")) {
            printf("noskipfix = true\n");
            noskipfix = true;
        } else if (!strcmp(argv[i], "-largebounds") || !strcmp(argv[i], "-lb")) {
             // qb: from kmqbsp3- Knightmare added
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
        } else if((!strcmp(argv[i], "-chop")) || (!strcmp(argv[i], "-subdiv"))) {
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
        } else if((!strcmp(argv[i], "-choplight")) || (!strcmp(argv[i], "-choplights")) || (!strcmp(argv[i], "-subdivlight"))) {
            // qb: chop surf lights independently
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
        } else if(!strcmp(argv[i], "-blocksize")) {
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
        } else if (!strcmp(argv[i], "-tmpin"))
            strcpy(inbase, "/tmp");
        else if (!strcmp(argv[i], "-tmpout"))
            strcpy(outbase, "/tmp");
        else if (!strcmp(argv[i], "-fast")) {
            printf("fastvis = true\n");
            fastvis = true;
        } else if (!strcmp(argv[i], "-nosort")) {
            printf("nosort = true\n");
            nosort = true;
        } else if (!strcmp(argv[i], "-dump")) {
                    printf("dicepatches = true\n");
            dumppatches = true;
        } else if (!strcmp(argv[i], "-bounce")) {
            numbounce = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-extra")) {
            extrasamples = true;
            printf("extrasamples = true\n");
        } else if (!strcmp(argv[i], "-noedgefix")) {
            // qb: light warp surfaces
            noedgefix = true;
            printf("no edge fix = true\n");
        } else if (!strcmp(argv[i], "-dice")) {
            dicepatches = true;
            printf("dicepatches = true\n");
        } else if (!strcmp(argv[i], "-threads")) {
            numthreads = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-maxdata")) { // qb: allows increase for some engines
            maxdata = atoi(argv[i + 1]);
            i++;
            if (maxdata > DEFAULT_MAP_LIGHTING) {
                printf("lighting maxdata (%i) exceeds typical limit (%i).\n", maxdata, DEFAULT_MAP_LIGHTING);
            }
        } else if (!strcmp(argv[i], "-scale")) {
            lightscale = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-sunradscale")) {
            sunradscale = atof(argv[i + 1]);
            if (sunradscale < 0) {
                sunradscale = 0;
                printf("sunradscale set to minimum: 0\n");
            }
            printf("sunradscale = %f\n", sunradscale);
            i++;
        } else if (!strcmp(argv[i], "-saturation")) {
            saturation = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-radmin")) {
            patch_cutoff = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-direct")) {
            direct_scale *= atof(argv[i + 1]);
            // printf ("direct light scaling at %f\n", direct_scale);
            i++;
        } else if (!strcmp(argv[i], "-entity")) {
            entity_scale *= atof(argv[i + 1]);
            // printf ("entity light scaling at %f\n", entity_scale);
            i++;
        } else if (!strcmp(argv[i], "-nopvs")) {
            nopvs = true;
            printf("nopvs = true\n");
        } else if (!strcmp(argv[i], "-noblock")) {
            noblock = true;
            printf("noblock = true\n");
        } else if (!strcmp(argv[i], "-smooth")) {
            // qb: limit range
            smoothing_value = BOUND(0, atof(argv[i + 1]), 90);
            i++;
        } else if (!strcmp(argv[i], "-nudge")) {
            sample_nudge = atof(argv[i + 1]);
            // qb: nah, go crazy.  sample_nudge = BOUND(0, sample_nudge, 1.0);
            i++;
        } else if (!strcmp(argv[i], "-ambient")) {
            ambient = BOUND(0, atof(argv[i + 1]), 255);
            i++;
        } else if (!strcmp(argv[i], "-savetrace")) {
            save_trace = true;
            printf("savetrace = true\n");
        } else if (!strcmp(argv[i], "-maxlight")) {
            maxlight = BOUND(0, atof(argv[i + 1]), 255);
        } else
            break;
    }

    for(; i < argc; i++) {
        size_t input_length = strlen(argv[i]);

        qboolean is_map = strcmp(argv[i] + input_length - 4, ".map") == 0;

        if(do_bsp) {
            if(!is_map)
                Error("bsp operation requires a map file input.");
        } else if(do_vis || do_rad) {
            if(is_map)
                Error("bsp operation requires a bsp file input.");
        } else {
            Error("no operations chosen to be performed on input.");
        }

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

        // qb: display dirs
        printf("moddir = %s\n", moddir);
        printf("basedir = %s\n", basedir);
        printf("gamedir = %s\n", gamedir);

        if(do_bsp) {
            int32_t old_numthreads = numthreads;
            // qb: below is from original source release.  On Windows, multi threads cause false leak errors.
            numthreads = 1; // multiple threads aren't helping...
            BSP_ProcessArgument(argv[i]);
            numthreads = old_numthreads;
        }
        if(do_vis || (do_bsp && do_rad && is_map)) {
            VIS_ProcessArgument(argv[i]);
        }
        if(do_rad) {
            RAD_ProcessArgument(argv[i]);
        }
    }
}
