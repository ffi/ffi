/*
 * Copyright (c) 2007 Wayne Meissner. All rights reserved.
 *
 * For licensing, see LICENSE.SPECS
 */

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep(x)
#endif

#ifndef _WIN32
#include <unistd.h>
#include <pthread.h>
#endif

int testAdd(int a, int b)
{
    return a + b;
};

int testFunctionAdd(int a, int b, int (*f)(int, int))
{
    return f(a, b);
};

// The first thread that starts running this function will set the
// flag variable to 1 and then wait for another thread to set it back
// to 0, which confirms that two copies of the function were running
// concurrently.  This function returns 0 for success or a non-zero
// error code from pthreads.
int testBlocking()
{
#ifndef _WIN32
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static volatile int flag = 0;
    int error;

    error = pthread_mutex_lock(&mutex);
    if (error) { return error; }

    if (flag)
    {
        flag = 0;
    }
    else
    {
        flag = 1;

        while(flag)
        {
            error = pthread_mutex_unlock(&mutex);
            if (error) { return error; }

            usleep(100);

            error = pthread_mutex_lock(&mutex);
            if (error) { return error; }
        }
    }

    error = pthread_mutex_unlock(&mutex);
    if (error) { return error; }
#endif
    return 0;
}

struct async_data {
    void (*fn)(int);
    int value;
};

static void* asyncThreadCall(void *data)
{
    struct async_data* d = (struct async_data *) data;
    if (d != NULL && d->fn != NULL) {
        (*d->fn)(d->value);
    }

    return NULL;
}

void testAsyncCallback(void (*fn)(int), int value)
{
#ifndef _WIN32
    pthread_t t;
    struct async_data d;
    d.fn = fn;
    d.value = value;
    pthread_create(&t, NULL, asyncThreadCall, &d);
    pthread_join(t, NULL);
#else
    (*fn)(value);
#endif
} 
