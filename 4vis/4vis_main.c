#include "stdlib.h"
#include "vis.h"
#include "threads.h"

extern qboolean fastvis;
extern qboolean nosort;
extern char inbase[32];
extern char outbase[32];

void VIS_ProcessArgument(const char * arg);

int32_t main(int32_t argc, char **argv) {

    int32_t i;
    char tgamedir[1024] = "", tbasedir[1024] = "", tmoddir[1024] = "";

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
                   "    -moddir [path]: Set a mod directory. Default is parent dir of map file.\n"
                   "    -basedir [path]: Set the directory for assets not in moddir. Default is moddir.\n"
                   "    -gamedir [path]: Set game directory, the folder with game executable.\n"
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
        }

        // qb:  set gamedir and basedir
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

        else if (!strcmp(argv[i], "-tmpin"))
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
               "    -basedir [path]         -gamedir [path]                 \n"
               "    -tmpin                  -tmpout               -v (verbose)\n\n");
        exit(1);
    }

    ThreadSetDefault();

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

    VIS_ProcessArgument(argv[i]);

    return 0;
}
