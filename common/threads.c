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

#include "cmdlib.h"
#include "threads.h"

#define MAX_THREADS 64

int32_t dispatch;
int32_t workcount;
int32_t oldf;
qboolean pacifier;

qboolean threaded;

/*
=============
GetThreadWork

=============
*/
int32_t GetThreadWork(void) {
    int32_t r;
    int32_t f;

    ThreadLock();

    if (dispatch == workcount) {
        ThreadUnlock();
        return -1;
    }

    f = 10 * dispatch / workcount;
    if (f != oldf) {
        oldf = f;
        if (pacifier) {
            printf("%i...", f);
            fflush(stdout);
        }
    }

    r = dispatch;
    dispatch++;
    ThreadUnlock();

    return r;
}

void (*workfunction)(int32_t);

void ThreadWorkerFunction(int32_t threadnum) {
    int32_t work;

    while (1) {
        work = GetThreadWork();
        if (work == -1)
            break;
        // printf ("thread %i, work %i\n", threadnum, work);
        workfunction(work);
    }
}

void RunThreadsOnIndividual(int32_t workcnt, qboolean showpacifier, void (*func)(int32_t)) {
    if (numthreads == -1)
        ThreadSetDefault();
    workfunction = func;
    RunThreadsOn(workcnt, showpacifier, ThreadWorkerFunction);
}

#ifdef USE_PTHREADS

#ifdef WIN32

#define USED

#include <windows.h>

int32_t numthreads = -1;
CRITICAL_SECTION crit;
static int32_t enter;

void ThreadSetDefault(void) {
    SYSTEM_INFO info;

    if (numthreads == -1) // not set manually
    {
        GetSystemInfo(&info);
        numthreads = info.dwNumberOfProcessors;
        if (numthreads < 1 || numthreads > 32)
            numthreads = 1;
    }

    qprintf("%i threads\n", numthreads);
}

void ThreadLock(void) {
    if (!threaded)
        return;
    EnterCriticalSection(&crit);
    if (enter)
        Error("Recursive ThreadLock\n");
    enter = 1;
}

void ThreadUnlock(void) {
    if (!threaded)
        return;
    if (!enter)
        Error("ThreadUnlock without lock\n");
    enter = 0;
    LeaveCriticalSection(&crit);
}

/*
=============
RunThreadsOn
=============
*/
void RunThreadsOn(int32_t workcnt, qboolean showpacifier, void (*func)(int32_t)) {
    int32_t threadid[MAX_THREADS];
    HANDLE threadhandle[MAX_THREADS];
    int32_t i;
    int32_t start, end;

    start     = I_FloatTime();
    dispatch  = 0;
    workcount = workcnt;
    oldf      = -1;
    pacifier  = showpacifier;
    threaded  = true;

    //
    // run threads in parallel
    //
    InitializeCriticalSection(&crit);

    if (numthreads == 1) { // use same thread
        func(0);
    } else {
        for (i = 0; i < numthreads; i++) {
            threadhandle[i] = CreateThread(
                NULL,                         // LPSECURITY_ATTRIBUTES lpsa,
                0,                            // DWORD cbStack,
                (LPTHREAD_START_ROUTINE)func, // LPTHREAD_START_ROUTINE lpStartAddr,
                (LPVOID)i,                    // LPVOID lpvThreadParm,
                0,                            //   DWORD fdwCreate,
                &threadid[i]);
        }

        for (i = 0; i < numthreads; i++)
            WaitForSingleObject(threadhandle[i], INFINITE);
    }
    DeleteCriticalSection(&crit);

    threaded = false;
    end      = I_FloatTime();
    if (pacifier)
        printf(" (%i)\n", end - start);
}

#else
#define USED

int32_t numthreads = 4;

void ThreadSetDefault(void) {
    if (numthreads == -1) // not set manually
    {
        numthreads = 4;
    }
}

#include <pthread.h>

pthread_mutex_t *my_mutex;

void ThreadLock(void) {
    if (my_mutex)
        pthread_mutex_lock(my_mutex);
}

void ThreadUnlock(void) {
    if (my_mutex)
        pthread_mutex_unlock(my_mutex);
}

/*
=============
RunThreadsOn
=============
*/
void RunThreadsOn(int32_t workcnt, qboolean showpacifier, void (*func)(int32_t)) {
    int32_t i;
    pthread_t work_threads[MAX_THREADS];
    void *status;
    pthread_attr_t attrib;
    pthread_mutexattr_t mattrib;
    int32_t start, end;

    start     = I_FloatTime();
    dispatch  = 0;
    workcount = workcnt;
    oldf      = -1;
    pacifier  = showpacifier;
    threaded  = true;

    if (pacifier)
        setbuf(stdout, NULL);

    if (!my_mutex) {
        my_mutex = malloc(sizeof(*my_mutex));
        if (pthread_mutexattr_init(&mattrib) == -1)
            Error("pthread_mutex_attr_create failed");
        if (pthread_mutex_init(my_mutex, &mattrib) == -1)
            Error("pthread_mutex_init failed");
    }

    if (pthread_attr_init(&attrib) == -1)
        Error("pthread_attr_create failed");
    if (pthread_attr_setstacksize(&attrib, 0x1000000) == -1)
        Error("pthread_attr_setstacksize failed");

    for (i = 0; i < numthreads; i++) {
        if (pthread_create(&work_threads[i], &attrib, (void *)func, &i) == -1)
            Error("pthread_create failed");
    }

    for (i = 0; i < numthreads; i++) {
        if (pthread_join(work_threads[i], &status) == -1)
            Error("pthread_join failed");
    }

    threaded = false;
    end      = I_FloatTime();
    if (pacifier)
        printf(" (%i)\n", end - start);
}

#endif
#endif

/*
=======================================================================

  SINGLE THREAD

=======================================================================
*/

#ifndef USED

int32_t numthreads = 1;

void ThreadSetDefault(void) {
    numthreads = 1;
}

void ThreadLock(void) {
}

void ThreadUnlock(void) {
}

/*
=============
RunThreadsOn
=============
*/
void RunThreadsOn(int32_t workcnt, qboolean showpacifier, void (*func)(int32_t)) {
    int32_t start, end;

    dispatch  = 0;
    workcount = workcnt;
    oldf      = -1;
    pacifier  = showpacifier;
    start     = I_FloatTime();
#ifdef NeXT
    if (pacifier)
        setbuf(stdout, NULL);
#endif
    func(0);

    end = I_FloatTime();
    if (pacifier)
        printf(" (%i)\n", end - start);
}

#endif
