/*
 * Copyright (c) 2026 FFI contributors
 *
 * For licensing, see LICENSE.SPECS
 */

#ifndef _WIN32
#include <pthread.h>
#include <unistd.h>

static pthread_mutex_t detachedThreadMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t detachedThreadStarted = PTHREAD_COND_INITIALIZER;
static int detachedThreadCount = 0;

static void*
detachedThreadLoop(void* data)
{
    pthread_mutex_lock(&detachedThreadMutex);
    detachedThreadCount++;
    pthread_cond_signal(&detachedThreadStarted);
    pthread_mutex_unlock(&detachedThreadMutex);

    for (;;) {
        usleep(100);
    }

    return NULL;
}

/* Spawn threads that remain inside this library until the process ends. */
int
startDetachedThreads(int count)
{
    int created = 0;
    int i;
    int target;

    pthread_mutex_lock(&detachedThreadMutex);
    target = detachedThreadCount;

    for (i = 0; i < count; i++) {
        pthread_t thread;
        if (pthread_create(&thread, NULL, detachedThreadLoop, NULL) == 0) {
            pthread_detach(thread);
            created++;
        }
    }

    target += created;
    while (detachedThreadCount < target) {
        pthread_cond_wait(&detachedThreadStarted, &detachedThreadMutex);
    }
    pthread_mutex_unlock(&detachedThreadMutex);

    return created;
}
#endif
