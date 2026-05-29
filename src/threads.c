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
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#else
    #include <unistd.h> // Required for sysconf()
#endif

#define MAX_THREADS 128  // Array sizing upper bound; actual max is computed from CPU count

static int32_t max_threads_computed = -1;  // Computed at first use: num_cpus - 1

static int32_t GetMaxThreads(void) {
    if (max_threads_computed >= 0)
        return max_threads_computed;

#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    int32_t num_cpus = (int32_t)info.dwNumberOfProcessors;
#else
    long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
#endif

    if (num_cpus < 1)
        num_cpus = 1;

    // Reserve one CPU as buffer
    max_threads_computed = (num_cpus > 1) ? (int32_t)(num_cpus - 1) : 1;

    // Cap to array bounds for safety
    if (max_threads_computed > MAX_THREADS)
        max_threads_computed = MAX_THREADS;

    return max_threads_computed;
}

volatile int32_t dispatch;
volatile int32_t workcount;
volatile int32_t oldf;
volatile bool pacifier;

volatile bool threaded;
volatile long threads_running = 0; // actual worker threads currently running

/*
=============
GetThreadWork

=============
*/
int32_t GetThreadWork(void) {
    int32_t r;
    int32_t f;

    ThreadLock();
    
     if (dispatch >= workcount) {
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

void RunThreadsOnIndividual(int32_t workcnt, bool showpacifier, void (*func)(int32_t)) {
    ThreadSetDefault();
    workfunction = func;
    RunThreadsOn(workcnt, showpacifier, ThreadWorkerFunction);
}

void ThreadSetDefault(void) {
    int32_t max_threads = GetMaxThreads();
    if (numthreads == -1 || numthreads > max_threads) // not set manually or exceeds maximum
    {
        numthreads = max_threads;
    }
    printf("Using %i processor threads\n", numthreads);
}

#ifdef _WIN32

#define USED

#include <windows.h>

int32_t numthreads = -1;
CRITICAL_SECTION crit;
static int32_t enter;

static DWORD WINAPI ThreadStartWin32(LPVOID param) {
    int32_t threadnum = (int32_t)(intptr_t)param;
    InterlockedIncrement(&threads_running);
    qprintf("thread start (win): %d\n", threadnum);
    ThreadWorkerFunction(threadnum);
    qprintf("thread exit  (win): %d\n", threadnum);
    InterlockedDecrement(&threads_running);
    return 0;
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
void RunThreadsOn(int32_t workcnt, bool showpacifier, void (*func)(int32_t)) {
    int32_t threadid[MAX_THREADS];
    HANDLE threadhandle[MAX_THREADS];
    int32_t i;
    int32_t start, end;

    int32_t threadcount;

    start     = I_FloatTime();
    dispatch  = 0;
    workcount = workcnt;
    oldf      = -1;
    pacifier  = showpacifier;
    threaded  = true;
    enter     = 0;  // Reset lock state for this run

    threadcount = numthreads;
    if (workcnt > 0 && threadcount > workcnt)
        threadcount = workcnt;
    if (threadcount < 1)
        threadcount = 1;

    //
    // run threads in parallel
    //
    InitializeCriticalSection(&crit);
    threads_running = 0;

    if (threadcount == 1) { // use same thread
        InterlockedIncrement(&threads_running);
        qprintf("thread start (main): 0\n");
        func(0);
        qprintf("thread exit  (main): 0\n");
        InterlockedDecrement(&threads_running);
    } else {
        for (i = 0; i < threadcount; i++) {
            threadhandle[i] = CreateThread(
                NULL,                         // LPSECURITY_ATTRIBUTES lpsa,
                0,                            // DWORD cbStack,
                ThreadStartWin32,             // LPTHREAD_START_ROUTINE lpStartAddr,
                (LPVOID)(intptr_t)i,          // LPVOID lpvThreadParm,
                0,                            // DWORD fdwCreate,
                (LPDWORD)&threadid[i]);
            if (!threadhandle[i])
                Error("CreateThread failed");
        }

        /* Wait briefly for threads to start and report how many actually started */
        {
            int tries = 0;
            while (threads_running < threadcount && tries < 100) {
                Sleep(10);
                tries++;
            }
            qprintf("threads in-use: %d\n", (int)threads_running);
        }

        for (i = 0; i < threadcount; i++)
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

#include <pthread.h>
#include <unistd.h>

int32_t numthreads = -1;
static pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *ThreadStartPthread(void *param) {
    int32_t threadnum = (int32_t)(intptr_t)param;
    __sync_add_and_fetch(&threads_running, 1);
    qprintf("thread start (pthread): %d\n", threadnum);
    ThreadWorkerFunction(threadnum);
    qprintf("thread exit  (pthread): %d\n", threadnum);
    __sync_sub_and_fetch(&threads_running, 1);
    return NULL;
}

void ThreadLock(void) {
    pthread_mutex_lock(&my_mutex);
}

void ThreadUnlock(void) {
    pthread_mutex_unlock(&my_mutex);
}

/*
============= 
RunThreadsOn
=============
*/
void RunThreadsOn(int32_t workcnt, bool showpacifier, void (*func)(int32_t)) {
    int32_t i;
    pthread_t work_threads[MAX_THREADS];
    void *status;
    pthread_attr_t attrib;
    int32_t start, end;

    int32_t threadcount;

    start     = I_FloatTime();
    dispatch  = 0;
    workcount = workcnt;
    oldf      = -1;
    pacifier  = showpacifier;
    threaded  = true;

    threadcount = numthreads;
    if (workcnt > 0 && threadcount > workcnt)
        threadcount = workcnt;
    if (threadcount < 1)
        threadcount = 1;

    if (pacifier)
        setbuf(stdout, NULL);

    if (pthread_attr_init(&attrib) != 0)
        Error("pthread_attr_create failed");
    if (pthread_attr_setstacksize(&attrib, 0x1000000) != 0)
        Error("pthread_attr_setstacksize failed");

    threads_running = 0;
    for (i = 0; i < threadcount; i++) {
        if (pthread_create(&work_threads[i], &attrib, ThreadStartPthread, (void *)(intptr_t)i) != 0)
            Error("pthread_create failed");
    }

    /* Wait briefly for threads to start and report how many actually started */
    {
        int tries = 0;
        while (threads_running < threadcount && tries < 100) {
            usleep(10000);
            tries++;
        }
        qprintf("threads in-use: %d\n", (int)threads_running);
    }

    for (i = 0; i < threadcount; i++) {
        if (pthread_join(work_threads[i], &status) != 0)
            Error("pthread_join failed");
    }

    pthread_attr_destroy(&attrib);
    threaded = false;
    end      = I_FloatTime();
    if (pacifier)
        printf(" (%i)\n", end - start);
}

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
void RunThreadsOn(int32_t workcnt, bool showpacifier, void (*func)(int32_t)) {
    int32_t start, end;

    dispatch  = 0;
    workcount = workcnt;
    oldf      = -1;
    pacifier  = showpacifier;
    start     = I_FloatTime();

    func(0);

    end = I_FloatTime();
    if (pacifier)
        printf(" (%i)\n", end - start);
}

#endif
