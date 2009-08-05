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
#ifdef _WIN32
  typedef char* caddr_t;
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
#  define METHOD_PARAMS void**
#endif

static void* allocatePage(void);
static bool freePage(void *);
static bool protectPage(void *);

typedef void (*methodfn)(ffi_cif* cif, void* retval, METHOD_PARAMS parameters, void* user_data);
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

typedef struct BlockingCall_ {
    void* function;
    FunctionInfo* info;
    void **ffiValues;
    FFIStorage* retval;
} BlockingCall;

struct MethodHandlePool {
#if defined (HAVE_NATIVETHREAD) && !defined(_WIN32)
    pthread_mutex_t mutex;
#endif
    void (*fn)(ffi_cif* cif, void* retval, METHOD_PARAMS parameters, void* user_data);
    MethodHandle* list;
};

static ffi_type* methodHandleParamTypes[]= {
    &ffi_type_sint,
    &ffi_type_pointer,
    &ffi_type_ulong,
};

static MethodHandlePool defaultMethodHandlePool, fastMethodHandlePool;

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

    arity = fnInfo->parameterCount;
    if (arity < 0) {
        rb_raise(rb_eRuntimeError, "Cannot create method handle for variadic functions");
        return NULL;
    }

    if (arity <= MAX_METHOD_FIXED_ARITY && !fnInfo->blocking && !fnInfo->hasStruct) {
        pool = &fastMethodHandlePool;
    } else {
        pool = &defaultMethodHandlePool;
    }

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
    page = allocatePage();
    if (page == NULL) {
        pool_unlock(pool);
        rb_raise(rb_eRuntimeError, "failed to allocate a page. errno=%d (%s)", errno, strerror(errno));
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
        ffiStatus = ffi_prep_raw_closure(method->closure, &method->cif, pool->fn, method);
#else
        ffiStatus = ffi_prep_closure(method->closure, &method->cif, pool->fn, method);
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
        freePage(page);
        pool_unlock(pool);
        rb_raise(rb_eRuntimeError, "%s", errmsg);
    }
    protectPage(page);

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


#if defined(HAVE_NATIVETHREAD)
static VALUE
call_blocking_function(void* data)
{
    BlockingCall* b = (BlockingCall *) data;

    ffi_call(&b->info->ffi_cif, FFI_FN(b->function), b->retval, b->ffiValues);

    return Qnil;
}

#endif

/*
 * attached_method_invoke is used as the <= MAX_METHOD_FIXED_ARITY argument fixed-arity fast path
 */
static void
attached_method_fast_invoke(ffi_cif* cif, void* mretval, METHOD_PARAMS parameters, void* user_data)
{
    MethodHandle* handle =  (MethodHandle *) user_data;
    FunctionInfo* fnInfo = handle->info;
    void* ffiValues[MAX_METHOD_FIXED_ARITY];
    FFIStorage params[MAX_METHOD_FIXED_ARITY], retval;


    if (fnInfo->parameterCount > 0) {
#ifdef USE_RAW
        int argc = parameters[0].sint;
        VALUE* argv = *(VALUE **) & parameters[1];
#else
        int argc = *(ffi_sarg *) parameters[0];
        VALUE* argv = *(VALUE **) parameters[1];
#endif

        rbffi_SetupCallParams(argc, argv,
                fnInfo->parameterCount, fnInfo->nativeParameterTypes, params, ffiValues,
                fnInfo->callbackParameters, fnInfo->callbackCount, fnInfo->rbEnums);
    }

#ifdef USE_RAW
    ffi_raw_call(&fnInfo->ffi_cif, FFI_FN(handle->function), &retval, (ffi_raw *) ffiValues[0]);
#else
    ffi_call(&fnInfo->ffi_cif, FFI_FN(handle->function), &retval, ffiValues);
#endif

    if (!fnInfo->ignoreErrno) {
        rbffi_save_errno();
    }

    *((VALUE *) mretval) = rbffi_NativeValue_ToRuby(fnInfo->returnType, fnInfo->rbReturnType,
        &retval, fnInfo->rbEnums);
}

/*
 * attached_method_vinvoke is used functions with more than 3 parameters
 */
static void
attached_method_invoke(ffi_cif* cif, void* mretval, METHOD_PARAMS parameters, void* user_data)
{
    MethodHandle* handle =  (MethodHandle *) user_data;
    FunctionInfo* fnInfo = handle->info;
    void** ffiValues = ALLOCA_N(void *, fnInfo->parameterCount);
    FFIStorage* params = ALLOCA_N(FFIStorage, fnInfo->parameterCount);
    void* retval;

#ifdef USE_RAW
    int argc = parameters[0].sint;
    VALUE* argv = *(VALUE **) &parameters[1];
#else
    int argc = *(ffi_sarg *) parameters[0];
    VALUE* argv = *(VALUE **) parameters[1];
#endif

    retval = alloca(MAX(fnInfo->ffi_cif.rtype->size, FFI_SIZEOF_ARG));
    
    rbffi_SetupCallParams(argc, argv,
        fnInfo->parameterCount, fnInfo->nativeParameterTypes, params, ffiValues,
        fnInfo->callbackParameters, fnInfo->callbackCount, fnInfo->rbEnums);

#if defined(HAVE_NATIVETHREAD)
    if (unlikely(fnInfo->blocking)) {
        BlockingCall bc;
        bc.info = fnInfo;
        bc.function = handle->function;
        bc.ffiValues = ffiValues;
        bc.retval = retval;
        rb_thread_blocking_region(call_blocking_function, &bc, NULL, NULL);
    } else {
        ffi_call(&fnInfo->ffi_cif, FFI_FN(handle->function), retval, ffiValues);
    }
#else
    ffi_call(&fnInfo->ffi_cif, FFI_FN(handle->function), retval, ffiValues);
#endif

    if (!fnInfo->ignoreErrno) {
        rbffi_save_errno();
    }

    *((VALUE *) mretval) = rbffi_NativeValue_ToRuby(fnInfo->returnType, fnInfo->rbReturnType, retval,
        fnInfo->rbEnums);
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

static void*
allocatePage(void)
{
#ifdef _WIN32
    return VirtualAlloc(NULL, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    caddr_t page = mmap(NULL, pageSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    return (page != (caddr_t) -1) ? page : NULL;
#endif
}

static bool
freePage(void *addr)
{
#ifdef _WIN32
    return VirtualFree(addr, 0, MEM_RELEASE);
#else
    munmap(addr, pageSize) == 0;
#endif
}

static bool
protectPage(void* page)
{
#ifdef _WIN32
    DWORD oldProtect;
    return VirtualProtect(page, pageSize, PAGE_EXECUTE_READ, &oldProtect);
#else
    return mprotect(page, pageSize, PROT_READ | PROT_EXEC) == 0;
#endif
}

void
rbffi_MethodHandle_Init(VALUE module)
{
    pageSize = getPageSize();

#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32)
    pthread_mutex_init(&defaultMethodHandlePool.mutex, NULL);
    pthread_mutex_init(&fastMethodHandlePool.mutex, NULL);
#endif /* USE_PTHREAD_LOCAL */
    defaultMethodHandlePool.fn = attached_method_invoke;
    fastMethodHandlePool.fn = attached_method_fast_invoke;
}

