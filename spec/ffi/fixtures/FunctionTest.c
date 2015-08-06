/*
 * Copyright (c) 2007 Wayne Meissner. All rights reserved.
 *
 * For licensing, see LICENSE.SPECS
 */

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>

int testAdd(int a, int b)
{
    return a + b;
};

int testFunctionAdd(int a, int b, int (*f)(int, int))
{
    return f(a, b);
};

struct testBlockingData {
    int pipe1[2];
    int pipe2[2];
};

struct testBlockingData *testBlockingOpen()
{
    struct testBlockingData *self = malloc(sizeof(struct testBlockingData));

    if( pipe(self->pipe1) == -1 ) return NULL;
    if( pipe(self->pipe2) == -1 ) return NULL;
    return self;
}

char testBlockingReadChar(int fd)
{
    char d;
    struct timeval timeout;
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    timeout.tv_sec = 10; // timeout after x seconds
    timeout.tv_usec = 0;

    if(select(fd + 1, &read_fds, NULL, NULL, &timeout) <= 0)
      return 0;

    if( read(fd, &d, 1) != 1)
      return 0;
    return d;
}

char testBlockingWR(struct testBlockingData *self, char c) {
    if( write(self->pipe1[1], &c, 1) != 1)
      return 0;
    return testBlockingReadChar(self->pipe2[0]);
}

char testBlockingRW(struct testBlockingData *self, char c) {
    char d = testBlockingReadChar(self->pipe1[0]);
    if( write(self->pipe2[1], &c, 1) != 1)
      return 0;
    return d;
}

void testBlockingClose(struct testBlockingData *self) {
    close(self->pipe1[0]);
    close(self->pipe1[1]);
    close(self->pipe2[0]);
    close(self->pipe2[1]);
    free(self);
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

#if defined(_WIN32) && !defined(_WIN64)
struct StructUCDP {
  unsigned char a1;
  double a2;
  void *a3;
};

void __stdcall testStdcallManyParams(long *a1, char a2, short int a3, int a4, __int64 a5,
            struct StructUCDP a6, struct StructUCDP *a7, float a8, double a9) {
}
#endif
