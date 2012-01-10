/*
 * Copyright (c) 2008, 2009, Wayne Meissner
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
#include <sys/param.h>
#endif
#include <sys/types.h>
#include <stdio.h>
#ifndef _MSC_VER
#include <stdint.h>
#include <stdbool.h>
#else
typedef int bool;
#define true 1
#define false 0
#endif
#include <errno.h>
#include <ruby.h>

#include "LastError.h"

#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32) && !defined(__WIN32__)
# include <pthread.h>
# define USE_PTHREAD_LOCAL
#endif

typedef struct ThreadData {
    int td_errno;
} ThreadData;

#if defined(USE_PTHREAD_LOCAL)
static pthread_key_t threadDataKey;
#endif

static inline ThreadData* thread_data_get(void);

#if defined(USE_PTHREAD_LOCAL)

static ThreadData*
thread_data_init(void)
{
    ThreadData* td = xcalloc(1, sizeof(ThreadData));

    pthread_setspecific(threadDataKey, td);

    return td;
}


static inline ThreadData*
thread_data_get(void)
{
    ThreadData* td = pthread_getspecific(threadDataKey);
    return td != NULL ? td : thread_data_init();
}

static void
thread_data_free(void *ptr)
{
    xfree(ptr);
}

#else
static ID id_thread_data;

static ThreadData*
thread_data_init(void)
{
    ThreadData* td;
    VALUE obj;

    obj = Data_Make_Struct(rb_cObject, ThreadData, NULL, -1, td);
    rb_thread_local_aset(rb_thread_current(), id_thread_data, obj);

    return td;
}

static inline ThreadData*
thread_data_get()
{
    VALUE obj = rb_thread_local_aref(rb_thread_current(), id_thread_data);

    if (obj != Qnil && TYPE(obj) == T_DATA) {
        return (ThreadData *) DATA_PTR(obj);
    }

    return thread_data_init();
}

#endif


/*
 * call-seq: error
 * @return [Numeric]
 * Get +errno+ value.
 */
static VALUE
get_last_error(VALUE self)
{
    return INT2NUM(thread_data_get()->td_errno);
}


/*
 * call-seq: error(error)
 * @param [Numeric] error
 * @return [nil]
 * Set +errno+ value.
 */
static VALUE
set_last_error(VALUE self, VALUE error)
{

#ifdef _WIN32
    SetLastError(NUM2INT(error));
#else
    errno = NUM2INT(error);
#endif

    return Qnil;
}


void
rbffi_save_errno(void)
{
    int error = 0;

#ifdef _WIN32
    error = GetLastError();
#else
    error = errno;
#endif

    thread_data_get()->td_errno = error;
}


void
rbffi_LastError_Init(VALUE moduleFFI)
{
    /*
     * Document-module: FFI::LastError
     * This module defines a couple of method to set and get +errno+
     * for current thread.
     */
    VALUE moduleError = rb_define_module_under(moduleFFI, "LastError");

    rb_define_module_function(moduleError, "error", get_last_error, 0);
    rb_define_module_function(moduleError, "error=", set_last_error, 1);

#if defined(USE_PTHREAD_LOCAL)
    pthread_key_create(&threadDataKey, thread_data_free);
#else
    id_thread_data = rb_intern("ffi_thread_local_data");
#endif /* USE_PTHREAD_LOCAL */
}

