/*
 * Copyright (c) 2009, Wayne Meissner
 * Copyright (c) 2009, Aman Gupta
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
# include <sys/mman.h>
#endif
#include <ruby.h>
#include <ffi.h>
#include "AbstractMemory.h"
#include "Pointer.h"
#include "MemoryPointer.h"
#include "Function.h"
#include "Callback.h"
#include "Types.h"
#include "Type.h"
#include "rbffi.h"
#include "compat.h"
#include "extconf.h"

#if defined(HAVE_LIBFFI) && !defined(HAVE_FFI_CLOSURE_ALLOC)
static void* ffi_closure_alloc(size_t size, void** code);
static void ffi_closure_free(void* ptr);
ffi_status ffi_prep_closure_loc(ffi_closure* closure, ffi_cif* cif,
        void (*fun)(ffi_cif*, void*, void**, void*),
        void* user_data, void* code);
#endif /* HAVE_FFI_CLOSURE_ALLOC */

static VALUE NativeCallbackClass = Qnil;
static ID id_call = 0, id_cbtable = 0;

static void
native_callback_free(NativeCallback* cb)
{
    if (cb->ffi_closure != NULL) {
        ffi_closure_free(cb->ffi_closure);
    }
    xfree(cb);
}

static void
native_callback_mark(NativeCallback* cb)
{
    rb_gc_mark(cb->rbFunctionInfo);
    rb_gc_mark(cb->rbProc);
}

static void
native_callback_invoke(ffi_cif* cif, void* retval, void** parameters, void* user_data)
{
    NativeCallback* cb = (NativeCallback *) user_data;
    FunctionType *cbInfo = cb->cbInfo;
    VALUE* rbParams;
    VALUE rbReturnValue;
    int i;

    rbParams = ALLOCA_N(VALUE, cbInfo->parameterCount);
    for (i = 0; i < cbInfo->parameterCount; ++i) {
        VALUE param;
        switch (cbInfo->parameterTypes[i]->nativeType) {
            case NATIVE_INT8:
                param = INT2NUM(*(int8_t *) parameters[i]);
                break;
            case NATIVE_UINT8:
                param = UINT2NUM(*(uint8_t *) parameters[i]);
                break;
            case NATIVE_INT16:
                param = INT2NUM(*(int16_t *) parameters[i]);
                break;
            case NATIVE_UINT16:
                param = UINT2NUM(*(uint16_t *) parameters[i]);
                break;
            case NATIVE_INT32:
                param = INT2NUM(*(int32_t *) parameters[i]);
                break;
            case NATIVE_UINT32:
                param = UINT2NUM(*(uint32_t *) parameters[i]);
                break;
            case NATIVE_INT64:
                param = LL2NUM(*(int64_t *) parameters[i]);
                break;
            case NATIVE_UINT64:
                param = ULL2NUM(*(uint64_t *) parameters[i]);
                break;
            case NATIVE_FLOAT32:
                param = rb_float_new(*(float *) parameters[i]);
                break;
            case NATIVE_FLOAT64:
                param = rb_float_new(*(double *) parameters[i]);
                break;
            case NATIVE_STRING:
                param = rb_tainted_str_new2(*(char **) parameters[i]);
                break;
            case NATIVE_POINTER:
                param = rbffi_Pointer_NewInstance(*(void **) parameters[i]);
                break;
            case NATIVE_BOOL:
                param = (*(void **) parameters[i]) ? Qtrue : Qfalse;
                break;

            case NATIVE_FUNCTION:
            case NATIVE_CALLBACK:
                param = rbffi_NativeValue_ToRuby(cbInfo->parameterTypes[i],
                     rb_ary_entry(cbInfo->rbParameterTypes, i), parameters[i], Qnil);
                break;
            default:
                param = Qnil;
                break;
        }
        rbParams[i] = param;
    }
    rbReturnValue = rb_funcall2(cb->rbProc, id_call, cbInfo->parameterCount, rbParams);
    if (rbReturnValue == Qnil || TYPE(rbReturnValue) == T_NIL) {
        memset(retval, 0, cbInfo->ffiReturnType->size);
    } else switch (cbInfo->returnType->nativeType) {
        case NATIVE_INT8:
        case NATIVE_INT16:
        case NATIVE_INT32:
            *((ffi_sarg *) retval) = NUM2INT(rbReturnValue);
            break;
        case NATIVE_UINT8:
        case NATIVE_UINT16:
        case NATIVE_UINT32:
            *((ffi_arg *) retval) = NUM2UINT(rbReturnValue);
            break;
        case NATIVE_INT64:
            *((int64_t *) retval) = NUM2LL(rbReturnValue);
            break;
        case NATIVE_UINT64:
            *((uint64_t *) retval) = NUM2ULL(rbReturnValue);
            break;
        case NATIVE_FLOAT32:
            *((float *) retval) = (float) NUM2DBL(rbReturnValue);
            break;
        case NATIVE_FLOAT64:
            *((double *) retval) = NUM2DBL(rbReturnValue);
            break;
        case NATIVE_POINTER:
            if (TYPE(rbReturnValue) == T_DATA && rb_obj_is_kind_of(rbReturnValue, rbffi_PointerClass)) {
                *((void **) retval) = ((AbstractMemory *) DATA_PTR(rbReturnValue))->address;
            } else {
                // Default to returning NULL if not a value pointer object.  handles nil case as well
                *((void **) retval) = NULL;
            }
            break;
        case NATIVE_BOOL:
            *((ffi_sarg *) retval) = TYPE(rbReturnValue) == T_TRUE ? 1 : 0;
            break;

        case NATIVE_FUNCTION:
        case NATIVE_CALLBACK:
            if (TYPE(rbReturnValue) == T_DATA && rb_obj_is_kind_of(rbReturnValue, rbffi_PointerClass)) {

                *((void **) retval) = ((AbstractMemory *) DATA_PTR(rbReturnValue))->address;

            } else if (rb_obj_is_kind_of(rbReturnValue, rb_cProc) || rb_respond_to(rbReturnValue, id_call)) {
                VALUE callback;

                callback = rbffi_NativeCallback_ForProc(rbReturnValue, cbInfo->rbReturnType);
                
                *((void **) retval) = ((AbstractMemory *) DATA_PTR(callback))->address;
            } else {
                *((void **) retval) = NULL;
            }
            break;

        default:
            *((ffi_arg *) retval) = 0;
            break;
    }
}

static VALUE
native_callback_allocate(VALUE klass)
{
    NativeCallback* cb;
    VALUE obj;

    obj = Data_Make_Struct(klass, NativeCallback, native_callback_mark, native_callback_free, cb);
    cb->rbFunctionInfo = Qnil;
    cb->rbProc = Qnil;
    cb->base.address = NULL;
    cb->base.access = 0;
    cb->base.ops = NULL;
    
    return obj;
}

VALUE
rbffi_NativeCallback_NewInstance(VALUE rbFunctionInfo, VALUE rbProc)
{
    NativeCallback* closure = NULL;
    FunctionType* cbInfo;
    VALUE obj;
    ffi_status status;

    Data_Get_Struct(rbFunctionInfo, FunctionType, cbInfo);

    obj = Data_Make_Struct(NativeCallbackClass, NativeCallback, native_callback_mark, native_callback_free, closure);
    closure->cbInfo = cbInfo;
    closure->rbProc = rbProc;
    closure->rbFunctionInfo = rbFunctionInfo;

    closure->ffi_closure = ffi_closure_alloc(sizeof(*closure->ffi_closure), &closure->code);
    if (closure->ffi_closure == NULL) {
        rb_raise(rb_eNoMemError, "Failed to allocate FFI native closure");
    }

    status = ffi_prep_closure_loc(closure->ffi_closure, &cbInfo->ffi_cif,
            native_callback_invoke, closure, closure->code);
    if (status != FFI_OK) {
        rb_raise(rb_eArgError, "ffi_prep_closure_loc failed");
    }

    closure->base.address = closure->code;
    closure->base.size = LONG_MAX;
    closure->base.access = MEM_RD | MEM_CODE;
    
    return obj;
}

VALUE
rbffi_NativeCallback_ForProc(VALUE proc, VALUE cbInfo)
{
    VALUE callback;
    VALUE cbTable = RTEST(rb_ivar_defined(proc, id_cbtable)) ? rb_ivar_get(proc, id_cbtable) : Qnil;
    if (cbTable == Qnil) {
        cbTable = rb_hash_new();
        rb_ivar_set(proc, id_cbtable, cbTable);
    }
    callback = rb_hash_aref(cbTable, cbInfo);
    if (callback != Qnil) {
        return callback;
    }
    callback = rbffi_NativeCallback_NewInstance(cbInfo, proc);
    rb_hash_aset(cbTable, cbInfo, callback);
    return callback;
}
#if defined(HAVE_LIBFFI) && !defined(HAVE_FFI_CLOSURE_ALLOC)
/*
 * versions of ffi_closure_alloc, ffi_closure_free and ffi_prep_closure_loc for older
 * system libffi versions.
 */
static void*
ffi_closure_alloc(size_t size, void** code)
{
    void* closure;
    closure = mmap(NULL, size, PROT_READ | PROT_WRITE,
             MAP_ANON | MAP_PRIVATE, -1, 0);
    if (closure == (void *) -1) {
        return NULL;
    }
    memset(closure, 0, size);
    *code = closure;
    return closure;
}

static void
ffi_closure_free(void* ptr)
{
    if (ptr != NULL && ptr != (void *) -1) {
        munmap(ptr, sizeof(ffi_closure));
    }
}

ffi_status
ffi_prep_closure_loc(ffi_closure* closure, ffi_cif* cif,
        void (*fun)(ffi_cif*, void*, void**, void*),
        void* user_data, void* code)
{
    ffi_status retval = ffi_prep_closure(closure, cif, fun, user_data);
    if (retval == FFI_OK) {
        mprotect(closure, sizeof(ffi_closure), PROT_READ | PROT_EXEC);
    }
    return retval;
}

#endif /* HAVE_FFI_CLOSURE_ALLOC */

void
rbffi_Callback_Init(VALUE moduleFFI)
{
    NativeCallbackClass = rb_define_class_under(moduleFFI, "NativeCallback", rbffi_PointerClass);
    rb_global_variable(&NativeCallbackClass);
    rb_define_alloc_func(NativeCallbackClass, native_callback_allocate);
    id_call = rb_intern("call");
    id_cbtable = rb_intern("@__ffi_callback_table__");
}
