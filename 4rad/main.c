#include "qrad.h"

extern qboolean dumppatches;
extern qboolean nopvs;
extern qboolean save_trace;
extern float patch_cutoff;
extern char inbase[32];
extern char outbase[32];

void RAD_ProcessArgument(const char * arg);

int32_t main(int32_t argc, char **argv) {
    int32_t i;

    char tgamedir[1024] = "", tbasedir[1024] = "", tmoddir[1024] = "";

    printf("\n\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< 4rad >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    printf("radiosity compiler build " __DATE__ "\n");

    verbose        = false;
    numthreads     = -1;
    maxdata        = DEFAULT_MAP_LIGHTING;
    step           = LMSTEP;
    dlightdata_ptr = dlightdata;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-dump"))
            dumppatches = true;
        else if (!strcmp(argv[i], "-bounce")) {
            numbounce = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-v")) {
            verbose = true;
        } else if (!strcmp(argv[i], "-help")) {
            printf("4rad with automatic phong and QBSP extended limit support.\n"
                   "Usage: 4rad [options] [mapname]\n\n"
                   "    -ambient #: Minimum light level.\n"
                   "         range:  0 to 255.\n"
                   "    -moddir [path]: Set a mod directory. Default is parent dir of map file.\n"
                   "    -basedir [path]: Set the directory for assets not in moddir. Default is moddir.\n"
                   "    -gamedir [path]: Set game directory, the folder with game executable.\n"
                   "    -bounce #: Max number of light bounces for radiosity.\n"
                   "    -dice: Subdivide patches with a global grid rather than per patch.\n"
                   "    -direct #: Direct light scale factor.\n"
                   "    -entity #: Entity light scale factor.\n"
                   "    -extra: Use extra samples to smooth lighting.\n"
                   "    -maxdata #: 2097152 is default max. Not needed for QBSP format.\n"
                   "         Increase requires a supporting engine.\n"
                   "    -maxlight #: Maximium light level.\n"
                   "         range:  0 to 255.\n"
                   "    -noedgefix: disable dark edges at sky fix. More of a hack, really.\n"
                   "    -nudge #: Nudge factor for samples. Distance fraction from center.\n"
                   "    -saturate #: Saturation factor of light bounced off surfaces.\n"
                   "    -scale #: Light intensity multiplier.\n"
                   "    -smooth #: Threshold angle (# and 180deg - #) for phong smoothing.\n"
                   "    -subdiv (or -chop) #: Maximum patch size.  Default: 64\n"
                   "    -sunradscale #: Sky light intensity scale when sun is active.\n"
                   "    -threads #:  Number of CPU cores to use.\n"
                   "Debugging tools:\n"
                   "    -dump: Dump patches to a text file.\n"
                   "    -noblock: Brushes don't block lighting path.\n"
                   "    -nopvs:  Don't do potential visibility set check.\n"
                   "    -savetrace: Test traces and report errors.\n"
                   "    -tmpin: Read from 'tmp' directory.\n"
                   "    -tmpout: Write to 'tmp' directory.\n"
                   "    -v: Verbose output for debugging.\n\n");
            printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< 4rad HELP >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n\n");

            exit(1);
        }

        else if (!strcmp(argv[i], "-extra")) {
            extrasamples = true;
            printf("extrasamples = true\n");
        } else if (!strcmp(argv[i], "-noedgefix")) // qb: light warp surfaces
        {
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
        }

        // qb:  set gamedir, moddir, and basedir
        else if (!strcmp(argv[i], "-gamedir")) {
            strcpy(tgamedir, argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-basedir")) {
            strcpy(tbasedir, argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-moddir")) {
            strcpy(tmoddir, argv[i + 1]);
            i++;
        }

        else if ((!strcmp(argv[i], "-chop")) || (!strcmp(argv[i], "-subdiv"))) {
            subdiv = atoi(argv[i + 1]);
            if (subdiv < 16) {
                subdiv = 16;
                printf("subdiv set to minimum: 16\n");
            } else if (subdiv > 1024) {
                subdiv = 1024;
                printf("subdiv set to maximum: 1024\n");
            }
            i++;
        }

        else if (!strcmp(argv[i], "-scale")) {
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
            i++;
        } else if (!strcmp(argv[i], "-tmpin"))
            strcpy(inbase, "/tmp");
        else if (!strcmp(argv[i], "-tmpout"))
            strcpy(outbase, "/tmp");
        else
            break;
    }

    if (i != argc - 1) {
        printf("Usage: 4rad [options] [mapname]\n"
               "    -ambient #            -bounce #\n"
               "    -dice                 -direct #           -entity #\n"
               "    -extra                -help               -maxdata #\n"
               "    -maxlight #           -noedgefix          -nudge #\n"
               "    -saturate #           -scale #            -smooth #\n"
               "    -subdiv               -sunradscale #      -threads #\n"
               "    -gamedir [path]       -basedir [path]     -moddir [path]\n"
               "Debugging tools:\n"
               "    -dump                 -noblock            -nopvs\n"
               "    -savetrace            -tmpin              -tmpout\n"
               "    -v (verbose)\n\n");
        exit(1);
    }

    printf("sample nudge: %f\n", sample_nudge);
    printf("ambient     : %f\n", ambient);
    printf("scale       : %f\n", lightscale);
    printf("maxlight    : %f\n", maxlight);
    printf("entity      : %f\n", entity_scale);
    printf("direct      : %f\n", direct_scale);
    printf("saturation  : %f\n", saturation);
    printf("bounce      : %d\n", numbounce);
    printf("radmin      : %f\n", patch_cutoff);
    printf("subdiv      : %f\n", subdiv);
    printf("smooth angle: %f\n", smoothing_value);
    printf("nudge       : %f\n", sample_nudge);
    printf("threads     : %d\n", numthreads);

    ThreadSetDefault();


    smoothing_threshold = (float)cos(smoothing_value * (Q_PI / 180.0));

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

    RAD_ProcessArgument(argv[i]);

    return 0;
}
