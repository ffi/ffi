/*
 * Copyright (c) 2010 Wayne Meissner
 *
 * All rights reserved.
 *
 * This file is part of ruby-ffi.
 *
 * This code is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * version 3 for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with this work.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MSC_VER
#include <stdbool.h>
#else
# include "win32/stdbool.h"
# include "win32/stdint.h"
#endif

#if defined(__CYGWIN__) || !defined(_WIN32)
# include <pthread.h>
# include <errno.h>
# include <signal.h>
# include <unistd.h>
#else
# include <winsock2.h>
# define _WINSOCKAPI_
# include <windows.h>
#endif
#include <fcntl.h>
#include "Thread.h"

#ifdef _WIN32
static volatile DWORD frame_thread_key = TLS_OUT_OF_INDEXES;
#else
static pthread_key_t thread_data_key;
struct thread_data {
    rbffi_frame_t* frame;
};
static inline struct thread_data* thread_data_get(void);

#endif

rbffi_frame_t*
rbffi_frame_current(void)
{
#ifdef _WIN32
    return (rbffi_frame_t *) TlsGetValue(frame_thread_key);
#else
    struct thread_data* td = (struct thread_data *) pthread_getspecific(thread_data_key);
    return td != NULL ? td->frame : NULL;
#endif
}

void 
rbffi_frame_push(rbffi_frame_t* frame)
{
    memset(frame, 0, sizeof(*frame));
    frame->has_gvl = true;
    frame->exc = Qnil;
    
#ifdef _WIN32
    frame->prev = TlsGetValue(frame_thread_key);
    TlsSetValue(frame_thread_key, frame);
#else
    frame->td = thread_data_get();
    frame->prev = frame->td->frame;
    frame->td->frame = frame;
#endif
}

void 
rbffi_frame_pop(rbffi_frame_t* frame)
{
#ifdef _WIN32
    TlsSetValue(frame_thread_key, frame->prev);
#else
    frame->td->frame = frame->prev;
#endif
}

#if !(defined(HAVE_RB_THREAD_BLOCKING_REGION) || defined(HAVE_RB_THREAD_CALL_WITHOUT_GVL))

#if !defined(_WIN32)

struct BlockingThread {
    pthread_t tid;
    VALUE (*fn)(void *);
    void *data;
    void (*ubf)(void *);
    void *data2;
    VALUE retval;
    int wrfd;
    int rdfd;
};

static void*
rbffi_blocking_thread(void* args)
{
    struct BlockingThread* thr = (struct BlockingThread *) args;
    char c = 1;
    VALUE retval;
    
    retval = (*thr->fn)(thr->data);
    
    pthread_testcancel();

    thr->retval = retval;
    
    write(thr->wrfd, &c, sizeof(c));

    return NULL;
}

static VALUE
wait_for_thread(void *data)
{
    struct BlockingThread* thr = (struct BlockingThread *) data;
    char c;
    
    if (read(thr->rdfd, &c, 1) < 1) {
        rb_thread_wait_fd(thr->rdfd);
        while (read(thr->rdfd, &c, 1) < 1 && rb_io_wait_readable(thr->rdfd) == Qtrue) {
            ;
        }
    }

    return Qnil;
}

static VALUE
cleanup_blocking_thread(void *data, VALUE exc)
{
    struct BlockingThread* thr = (struct BlockingThread *) data;

    if (thr->ubf != (void (*)(void *)) -1) {
        (*thr->ubf)(thr->data2);
    } else {
        pthread_kill(thr->tid, SIGVTALRM);
    }

    return exc;
}

VALUE
rbffi_thread_blocking_region(VALUE (*func)(void *), void *data1, void (*ubf)(void *), void *data2)
{
    struct BlockingThread* thr;
    int fd[2];
    VALUE exc;
    
    if (pipe(fd) < 0) {
        rb_raise(rb_eSystemCallError, "pipe(2) failed");
        return Qnil;
    }
    fcntl(fd[0], F_SETFL, fcntl(fd[0], F_GETFL) | O_NONBLOCK);

    thr = ALLOC_N(struct BlockingThread, 1);
    thr->rdfd = fd[0];
    thr->wrfd = fd[1];
    thr->fn = func;
    thr->data = data1;
    thr->ubf = ubf;
    thr->data2 = data2;
    thr->retval = Qnil;

    if (pthread_create(&thr->tid, NULL, rbffi_blocking_thread, thr) != 0) {
        close(fd[0]);
        close(fd[1]);
        xfree(thr);
        rb_raise(rb_eSystemCallError, "pipe(2) failed");
        return Qnil;
    }

    exc = rb_rescue2(wait_for_thread, (VALUE) thr, cleanup_blocking_thread, (VALUE) thr,
        rb_eException);
    
    pthread_join(thr->tid, NULL);
    close(fd[1]);
    close(fd[0]);
    xfree(thr);

    if (exc != Qnil) {
        rb_exc_raise(exc);
    }

    return thr->retval;
}

#else
/* win32 implementation */

struct BlockingThread {
    HANDLE tid;
    VALUE (*fn)(void *);
    void *data;
    void (*ubf)(void *);
    void *data2;
    VALUE retval;
    int wrfd;
    int rdfd;
};

static DWORD __stdcall
rbffi_blocking_thread(LPVOID args)
{
    struct BlockingThread* thr = (struct BlockingThread *) args;
    char c = 1;
    VALUE retval;

    retval = (*thr->fn)(thr->data);
    thr->retval = retval;

    write(thr->wrfd, &c, sizeof(c));

    return 0;
}

static VALUE
wait_for_thread(void *data)
{
    struct BlockingThread* thr = (struct BlockingThread *) data;
    char c, res;
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(thr->rdfd, &rfds);
    rb_thread_select(thr->rdfd + 1, &rfds, NULL, NULL, NULL);
    read(thr->rdfd, &c, 1);
    return Qnil;
}

static VALUE
cleanup_blocking_thread(void *data, VALUE exc)
{
    struct BlockingThread* thr = (struct BlockingThread *) data;

    if (thr->ubf != (void (*)(void *)) -1) {
        (*thr->ubf)(thr->data2);
    } else {
        TerminateThread(thr->tid, 0);
    }

    return exc;
}

VALUE
rbffi_thread_blocking_region(VALUE (*func)(void *), void *data1, void (*ubf)(void *), void *data2)
{
    struct BlockingThread* thr;
    int fd[2];
    VALUE exc;
    DWORD state;
    DWORD res;

    if (_pipe(fd, 1024, O_BINARY) == -1) {
        rb_raise(rb_eSystemCallError, "_pipe() failed");
        return Qnil;
    }

    thr = ALLOC_N(struct BlockingThread, 1);
    thr->rdfd = fd[0];
    thr->wrfd = fd[1];
    thr->fn = func;
    thr->data = data1;
    thr->ubf = ubf;
    thr->data2 = data2;
    thr->retval = Qnil;

    thr->tid = CreateThread(NULL, 0, rbffi_blocking_thread, thr, 0, NULL);
    if (!thr->tid) {
        close(fd[0]);
        close(fd[1]);
        xfree(thr);
        rb_raise(rb_eSystemCallError, "CreateThread() failed");
        return Qnil;
    }

    exc = rb_rescue2(wait_for_thread, (VALUE) thr, cleanup_blocking_thread, (VALUE) thr,
        rb_eException);

    /* The thread should be finished, already. */
    WaitForSingleObject(thr->tid, INFINITE);
    CloseHandle(thr->tid);
    close(fd[1]);
    close(fd[0]);
    xfree(thr);

    if (exc != Qnil) {
        rb_exc_raise(exc);
    }

    return thr->retval;
}

#endif /* !_WIN32 */

#endif /* HAVE_RB_THREAD_BLOCKING_REGION */

#ifndef _WIN32
static struct thread_data* thread_data_init(void);

static inline struct thread_data*
thread_data_get(void)
{
    struct thread_data* td = (struct thread_data *) pthread_getspecific(thread_data_key);
    return td != NULL ? td : thread_data_init();
}

static struct thread_data*
thread_data_init(void)
{
    struct thread_data* td = calloc(1, sizeof(struct thread_data));

    pthread_setspecific(thread_data_key, td);

    return td;
}

static void
thread_data_free(void *ptr)
{
    free(ptr);
}
#endif

void
rbffi_Thread_Init(VALUE moduleFFI)
{
#ifdef _WIN32
    frame_thread_key = TlsAlloc();
#else
    pthread_key_create(&thread_data_key, thread_data_free);    
#endif
}
