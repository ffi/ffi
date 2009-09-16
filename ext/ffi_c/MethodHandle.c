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
static ffi_status prep_trampoline(MethodHandle* method, char* errmsg, size_t errmsgsize);
static int trampoline_size(void);

#if defined(__x86_64__) && defined(__GNUC__)
# define CUSTOM_TRAMPOLINE 1
#endif

struct MethodHandle {
    FunctionType* info;
    void* function;
    METHOD_CLOSURE* closure;
    void* code;
    ffi_cif cif;
    struct MethodHandlePool* pool;
    MethodHandle* next;
};

struct MethodHandlePool {
#if defined (HAVE_NATIVETHREAD) && !defined(_WIN32)
    pthread_mutex_t mutex;
#endif
    void (*fn)(ffi_cif* cif, void* retval, METHOD_PARAMS parameters, void* user_data);
    MethodHandle* list;
};

static MethodHandlePool defaultMethodHandlePool, fastMethodHandlePool;
static int pageSize;


MethodHandle*
rbffi_MethodHandle_Alloc(FunctionType* fnInfo, void* function)
{
    MethodHandle* method, *list = NULL;
    MethodHandlePool* pool;
    caddr_t page;
    int nclosures, trampolineSize;
    int i, arity;

    arity = fnInfo->parameterCount;
    if (arity < 0) {
        rb_raise(rb_eRuntimeError, "Cannot create method handle for variadic functions");
        return NULL;
    }

    if (false && arity <= MAX_METHOD_FIXED_ARITY && !fnInfo->blocking && !fnInfo->hasStruct && fnInfo->invoke == rbffi_CallFunction) {
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

    trampolineSize = roundup(trampoline_size(), 8);
    nclosures = pageSize / trampolineSize;
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
        method->code = method->closure = (METHOD_CLOSURE *) (page + (i * trampolineSize));

        if (prep_trampoline(method, errmsg, sizeof(errmsg)) != FFI_OK) {
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
    if (method != NULL) {
        MethodHandlePool* pool = method->pool;
        pool_lock(pool);
        method->next = pool->list;
        pool->list = method;
        pool_unlock(pool);
    }
}

void*
rbffi_MethodHandle_CodeAddress(MethodHandle* handle)
{
    return handle->code;
}

#ifndef CUSTOM_TRAMPOLINE

static ffi_type* methodHandleParamTypes[] = {
    &ffi_type_sint,
    &ffi_type_pointer,
    &ffi_type_ulong,
};

static ffi_status
prep_trampoline(MethodHandle* method, char* errmsg, size_t errmsgsize)
{
    ffi_status ffiStatus = ffi_prep_cif(&method->cif, FFI_DEFAULT_ABI, 3, &ffi_type_ulong,
            methodHandleParamTypes);
    if (ffiStatus != FFI_OK) {
        snprintf(errmsg, errmsgsize, "ffi_prep_cif failed.  status=%#x", ffiStatus);
        return ffiStatus;
    }

#if defined(USE_RAW)
    ffiStatus = ffi_prep_raw_closure(method->closure, &method->cif, method->pool->fn, method);
#else
    ffiStatus = ffi_prep_closure(method->closure, &method->cif, method->pool->fn, method);
#endif
    if (ffiStatus != FFI_OK) {
        snprintf(errmsg, errmsgsize, "ffi_prep_closure failed.  status=%#x", ffiStatus);
        return ffiStatus;
    }

    return FFI_OK;
}

static int
trampoline_size(void)
{
    return sizeof(METHOD_CLOSURE);
}

#endif
/*
 * attached_method_invoke is used as the <= MAX_METHOD_FIXED_ARITY argument fixed-arity fast path
 */
static void
attached_method_fast_invoke(ffi_cif* cif, void* mretval, METHOD_PARAMS parameters, void* user_data)
{
    MethodHandle* handle =  (MethodHandle *) user_data;
    FunctionType* fnInfo = handle->info;
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
 * attached_method_vinvoke is used functions with more than 6 parameters, or
 * with struct param or return values
 */
static void
attached_method_invoke(ffi_cif* cif, void* mretval, METHOD_PARAMS parameters, void* user_data)
{
    MethodHandle* handle =  (MethodHandle *) user_data;
    
#ifdef USE_RAW
    int argc = parameters[0].sint;
    VALUE* argv = *(VALUE **) &parameters[1];
#else
    int argc = *(ffi_sarg *) parameters[0];
    VALUE* argv = *(VALUE **) parameters[1];
#endif

    *(VALUE *) mretval = (*handle->info->invoke)(argc, argv, handle->function, handle->info);
}

#if defined(CUSTOM_TRAMPOLINE)
#if defined(__x86_64__)

static VALUE custom_trampoline(int argc, VALUE* argv, VALUE self, MethodHandle*);

#define TRAMPOLINE_CTX_MAGIC (0xfee1deadcafebabe)
#define TRAMPOLINE_FUN_MAGIC (0xfeedfacebeeff00d)

/*
 * This is a hand-coded trampoline to speedup entry from ruby to the FFI translation
 * layer for x86_64 arches.
 *
 * Since a ruby function has exactly 3 arguments, and the first 6 arguments are
 * passed in registers for x86_64, we can tack on a context pointer by simply
 * putting a value in %rcx, then jumping to the C trampoline code.
 *
 * This results in approx a 30% speedup for x86_64 FFI dispatch
 */
asm(
    ".text\n\t"
    ".globl ffi_trampoline\n\t"
    ".globl _ffi_trampoline\n\t"
    "ffi_trampoline:\n\t"
    "_ffi_trampoline:\n\t"
    "movabsq $0xfee1deadcafebabe, %rcx\n\t"
    "movabsq $0xfee1deadcafebabe, %r11\n\t"
    "jmpq *%r11\n\t"
    ".globl ffi_trampoline_end\n\t"
    "ffi_trampoline_end:\n\t"
    ".globl _ffi_trampoline_end\n\t"
    "_ffi_trampoline_end:\n\t"
);

static VALUE
custom_trampoline(int argc, VALUE* argv, VALUE self, MethodHandle* handle)
{
    return (*handle->info->invoke)(argc, argv, handle->function, handle->info);
}

#elif defined(__i386__) && 0

static VALUE custom_trampoline(caddr_t args, MethodHandle*);
#define TRAMPOLINE_CTX_MAGIC (0xfee1dead)
#define TRAMPOLINE_FUN_MAGIC (0xbeefcafe)

/*
 * This is a hand-coded trampoline to speedup entry from ruby to the FFI translation
 * layer for i386 arches.
 *
 * This does not make a discernable difference vs a raw closure, so for now,
 * it is not enabled.
 */
asm(
    ".text\n\t"
    ".globl ffi_trampoline\n\t"
    ".globl _ffi_trampoline\n\t"
    "ffi_trampoline:\n\t"
    "_ffi_trampoline:\n\t"
    "subl    $12, %esp\n\t"
    "leal    16(%esp), %eax\n\t"
    "movl    %eax, (%esp)\n\t"
    "movl    $0xfee1dead, 4(%esp)\n\t"
    "movl    $0xbeefcafe, %eax\n\t"
    "call    *%eax\n\t"
    "addl    $12, %esp\n\t"
    "ret\n\t"
    ".globl ffi_trampoline_end\n\t"
    "ffi_trampoline_end:\n\t"
    ".globl _ffi_trampoline_end\n\t"
    "_ffi_trampoline_end:\n\t"
);

static VALUE
custom_trampoline(caddr_t args, MethodHandle* handle)
{
    return (*handle->info->invoke)(*(int *) args, *(VALUE **) (args + 4), handle->function, handle->info);
}

#endif /* __x86_64__ else __i386__ */

extern void ffi_trampoline(int argc, VALUE* argv, VALUE self);
extern void ffi_trampoline_end(void);
static int trampoline_offsets(int *, int *);

static int trampoline_ctx_offset, trampoline_func_offset;

static int
trampoline_offset(int off, const long value)
{
    caddr_t ptr;
    for (ptr = (caddr_t) &ffi_trampoline + off; ptr < (caddr_t) &ffi_trampoline_end; ++ptr) {
        if (*(long *) ptr == value) {
            return ptr - (caddr_t) &ffi_trampoline;
        }
    }

    return -1;
}

static int
trampoline_offsets(int* ctxOffset, int* fnOffset)
{
    *ctxOffset = trampoline_offset(0, TRAMPOLINE_CTX_MAGIC);
    if (*ctxOffset == -1) {
        return -1;
    }

    *fnOffset = trampoline_offset(0, TRAMPOLINE_FUN_MAGIC);
    if (*fnOffset == -1) {
        return -1;
    }

    return 0;
}

static ffi_status
prep_trampoline(MethodHandle* method, char* errmsg, size_t errmsgsize)
{
    caddr_t ptr = (caddr_t) method->code;

    memcpy(ptr, &ffi_trampoline, trampoline_size());
    // Patch the context and function addresses into the stub code
    *(intptr_t *)(ptr + trampoline_ctx_offset) = (intptr_t) method;
    *(intptr_t *)(ptr + trampoline_func_offset) = (intptr_t) custom_trampoline;

    return FFI_OK;
}

static int
trampoline_size(void)
{
    return (caddr_t) &ffi_trampoline_end - (caddr_t) &ffi_trampoline;
}

#endif /* CUSTOM_TRAMPOLINE */

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
    return munmap(addr, pageSize) == 0;
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

#if defined(CUSTOM_TRAMPOLINE)
    if (trampoline_offsets(&trampoline_ctx_offset, &trampoline_func_offset) != 0) {
        rb_raise(rb_eFatal, "Could not locate offsets in trampoline code");
    }
#endif
}

