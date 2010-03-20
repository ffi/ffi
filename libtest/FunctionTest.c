/*
 * Copyright (c) 2009 Wayne Meissner. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>

#ifdef __WIN32__
#include <windows.h>
#define sleep(x) Sleep(x)
#endif

#ifndef __WIN32__
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

void testBlocking(int seconds) {
    sleep(seconds);
};

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
#ifndef __WIN32__
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