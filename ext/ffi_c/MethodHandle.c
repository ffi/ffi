/*
 * Copyright (c) 2009, Wayne Meissner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * The name of the author or authors may not be used to endorse or promote
 *   products derived from this software without specific prior written permission.
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

#include <sys/param.h>
#include <sys/types.h>
#ifndef _WIN32
#  include <sys/mman.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#ifndef _WIN32
#  include <unistd.h>
#endif
#include <errno.h>
#include <ruby.h>
#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32) && !defined(__WIN32__)
# include <pthread.h>
#endif

#include <ffi.h>
#include "rbffi.h"
#include "compat.h"

#include "Function.h"
#include "Types.h"
#include "Type.h"
#include "LastError.h"
#include "Call.h"
#include "MethodHandle.h"


#define MAX_METHOD_FIXED_ARITY (6)

#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32) && !defined(__WIN32__)
#  define USE_PTHREAD_LOCAL
#endif

#ifndef roundup
#  define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif

#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32)
#  define pool_lock(p) pthread_mutex_lock(&(p)->mutex)
#  define pool_unlock(p)  pthread_mutex_unlock(&(p)->mutex)
#else
#  define pool_lock(p)
#  define pool_unlock(p)
#endif


#ifdef USE_RAW
#  define METHOD_CLOSURE ffi_raw_closure
#  define METHOD_PARAMS ffi_raw*
#else
#  define METHOD_CLOSURE ffi_closure
#  define METHOD_PARAMS void*
#endif

typedef void (*methodfn)(ffi_cif* cif, void* retval, METHOD_PARAMS parameters, void* user_data);
static void attached_method_invoke0(ffi_cif* cif, void* retval, METHOD_PARAMS parameters, void* user_data);
static void attached_method_fast_invoke(ffi_cif* cif, void* retval, METHOD_PARAMS parameters, void* user_data);
static void attached_method_invoke(ffi_cif* cif, void* retval, METHOD_PARAMS parameters, void* user_data);

struct MethodHandle {
    FunctionInfo* info;
    void* function;
    METHOD_CLOSURE* closure;
    void* code;
    ffi_cif cif;
    struct MethodHandlePool* pool;
    MethodHandle* next;
};

#if !defined(_WIN32) && !defined(__WIN32__)
struct MethodHandlePool {
#ifdef HAVE_NATIVETHREAD
    pthread_mutex_t mutex;
#endif
    MethodHandle* list;
};
#endif /* _WIN32 */

static ffi_type* methodHandleParamTypes[]= {
    &ffi_type_sint,
    &ffi_type_pointer,
    &ffi_type_ulong,
};

static MethodHandlePool methodHandlePool[MAX_METHOD_FIXED_ARITY + 1], defaultMethodHandlePool;

static int pageSize;


MethodHandle*
rbffi_MethodHandle_Alloc(FunctionInfo* fnInfo, void* function)
{
    MethodHandle* method, *list = NULL;
    MethodHandlePool* pool;
    caddr_t page;
    ffi_status ffiStatus;
    int nclosures, closureSize;
    int i, arity;
    void (*fn)(ffi_cif* cif, void* retval, METHOD_PARAMS parameters, void* user_data);

    arity = fnInfo->parameterCount;
    if (arity < 0) {
        rb_raise(rb_eRuntimeError, "Cannot create method handle for variadic functions");
        return NULL;
    }

    pool = (arity >= 0 && arity <= MAX_METHOD_FIXED_ARITY) ? &methodHandlePool[arity] : &defaultMethodHandlePool;
    pool_lock(pool);
    if (pool->list != NULL) {
        method = pool->list;
        pool->list = pool->list->next;
        pool_unlock(pool);
        method->info = fnInfo;
        method->function = function;

        return method;
    }

    closureSize = roundup(sizeof(METHOD_CLOSURE), 8);
    nclosures = pageSize / closureSize;
    page = mmap(NULL, pageSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (page == (caddr_t) -1) {
        pool_unlock(pool);
        rb_raise(rb_eRuntimeError, "mmap failed. errno=%d (%s)", errno, strerror(errno));
    }

    /* figure out which function to bounce the execution through */
    if (arity == 0) {
        fn = attached_method_invoke0;
    } else if (arity <= MAX_METHOD_FIXED_ARITY) {
        fn = attached_method_fast_invoke;
    } else {
        fn = attached_method_invoke;
    }

    for (i = 0; i < nclosures; ++i) {
        char errmsg[256];
        method = calloc(1, sizeof(MethodHandle));
        if (method == NULL) {
            snprintf(errmsg, sizeof(errmsg), "malloc failed: %s", strerror(errno));
            goto error;
        }
        method->next = list;
        list = method;
        method->pool = pool;
        method->code = method->closure = (METHOD_CLOSURE *) (page + (i * closureSize));

        ffiStatus = ffi_prep_cif(&method->cif, FFI_DEFAULT_ABI, 3, &ffi_type_ulong,
                    methodHandleParamTypes);
        if (ffiStatus != FFI_OK) {
            snprintf(errmsg, sizeof(errmsg), "ffi_prep_cif failed.  status=%#x", ffiStatus);
            goto error;
        }

#ifdef USE_RAW
        ffiStatus = ffi_prep_raw_closure(method->closure, &method->cif, fn, method);
#else
        ffiStatus = ffi_prep_closure(method->closure, &method->cif, fn, method);
#endif
        if (ffiStatus != FFI_OK) {
            snprintf(errmsg, sizeof(errmsg), "ffi_prep_closure failed.  status=%#x", ffiStatus);
            goto error;
        }
        continue;
error:
        while (list != NULL) {
            method = list;
            list = list->next;
            free(method);
        }
        munmap(page, pageSize);
        pool_unlock(pool);
        rb_raise(rb_eRuntimeError, "%s", errmsg);
    }
    mprotect(page, pageSize, PROT_READ | PROT_EXEC);

    /* take the first member of the list for this handle */
    method = list;
    list = list->next;

    /* now add the new block of MethodHandles to the pool */
    if (list != NULL) {
        list->next = pool->list;
        pool->list = list;
    }
    pool_unlock(pool);
    method->info = fnInfo;
    method->function = function;

    return method;
}

void
rbffi_MethodHandle_Free(MethodHandle* method)
{
    MethodHandlePool* pool = method->pool;
    pool_lock(pool);
    method->next = pool->list;
    pool->list = method;
    pool_unlock(pool);
}

void*
rbffi_MethodHandle_CodeAddress(MethodHandle* handle)
{
    return handle->code;
}

static inline VALUE
ffi_invoke(FunctionInfo* fnInfo, void* function, void** ffiValues)
{
    FFIStorage retval;

#ifdef USE_RAW
    ffi_raw_call(&fnInfo->ffi_cif, FFI_FN(function), &retval, (ffi_raw *) ffiValues[0]);
#else
    ffi_call(&fnInfo->ffi_cif, FFI_FN(function), &retval, ffiValues);
#endif

    if (!fnInfo->ignoreErrno) {
        rbffi_save_errno();
    }

    return rbffi_NativeValue_ToRuby(fnInfo->returnType, fnInfo->rbReturnType, &retval,
        fnInfo->rbEnums);
}

/*
 * attached_method_invoke0 is used for functions with no arguments
 */
static void
attached_method_invoke0(ffi_cif* cif, void* retval, METHOD_PARAMS parameters, void* user_data)
{
    MethodHandle* handle =  (MethodHandle *) user_data;
    FFIStorage params[1];
    void* ffiValues[] = { &params[0] };

    *((VALUE *) retval) = ffi_invoke(handle->info, handle->function, ffiValues);
}

/*
 * attached_method_invoke is used as the <= MAX_METHOD_FIXED_ARITY argument fixed-arity fast path
 */
static void
attached_method_fast_invoke(ffi_cif* cif, void* retval, METHOD_PARAMS parameters, void* user_data)
{
    MethodHandle* handle =  (MethodHandle *) user_data;
    FunctionInfo* fnInfo = handle->info;
    void* ffiValues[MAX_METHOD_FIXED_ARITY];
    FFIStorage params[MAX_METHOD_FIXED_ARITY];

#ifdef USE_RAW
    int argc = parameters[0].sint;
    VALUE* argv = *(VALUE **) &parameters[1];
#else
    int argc = *(ffi_sarg **) parameters[0];
    VALUE* argv = *(VALUE **) parameters[1];
#endif

    rbffi_SetupCallParams(argc, argv,
            fnInfo->parameterCount, fnInfo->nativeParameterTypes, params, ffiValues,
            fnInfo->callbackParameters, fnInfo->callbackCount, fnInfo->rbEnums);

    *((VALUE *) retval) = ffi_invoke(fnInfo, handle->function, ffiValues);
}

/*
 * attached_method_vinvoke is used functions with more than 3 parameters
 */
static void
attached_method_invoke(ffi_cif* cif, void* retval, METHOD_PARAMS parameters, void* user_data)
{
    MethodHandle* handle =  (MethodHandle *) user_data;
    FunctionInfo* fnInfo = handle->info;
    void** ffiValues = ALLOCA_N(void *, fnInfo->parameterCount);
    FFIStorage* params = ALLOCA_N(FFIStorage, fnInfo->parameterCount);

#ifdef USE_RAW
    int argc = parameters[0].sint;
    VALUE* argv = *(VALUE **) &parameters[1];
#else
    int argc = *(ffi_sarg **) parameters[0];
    VALUE* argv = *(VALUE **) parameters[1];
#endif

    rbffi_SetupCallParams(argc, argv,
        fnInfo->parameterCount, fnInfo->nativeParameterTypes, params, ffiValues,
        fnInfo->callbackParameters, fnInfo->callbackCount, fnInfo->rbEnums);

    *((VALUE *) retval) = ffi_invoke(fnInfo, handle->function, ffiValues);
}

static int
getPageSize()
{
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#else
    return sysconf(_SC_PAGESIZE);
#endif
}


void
rbffi_MethodHandle_Init(VALUE module)
{
#if defined(USE_PTHREAD_LOCAL)
    int i = 0;
#endif

    pageSize = getPageSize();

#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32)
    for (i = 0; i < 4; ++i) {
        pthread_mutex_init(&methodHandlePool[i].mutex, NULL);
    }
    pthread_mutex_init(&defaultMethodHandlePool.mutex, NULL);
#endif /* USE_PTHREAD_LOCAL */

}

