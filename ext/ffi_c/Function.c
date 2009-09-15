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

#include "MethodHandle.h"


#include <sys/param.h>
#include <sys/types.h>
#ifndef _WIN32
# include <sys/mman.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ruby.h>

#include <ffi.h>
#include "rbffi.h"
#include "compat.h"

#include "AbstractMemory.h"
#include "Pointer.h"
#include "Struct.h"
#include "Platform.h"
#include "Type.h"
#include "LastError.h"
#include "Call.h"
#include "Function.h"

#if defined(HAVE_LIBFFI) && !defined(HAVE_FFI_CLOSURE_ALLOC)
static void* ffi_closure_alloc(size_t size, void** code);
static void ffi_closure_free(void* ptr);
ffi_status ffi_prep_closure_loc(ffi_closure* closure, ffi_cif* cif,
        void (*fun)(ffi_cif*, void*, void**, void*),
        void* user_data, void* code);
#endif /* HAVE_FFI_CLOSURE_ALLOC */

typedef struct Function_ {
    AbstractMemory memory;
    FunctionType* info;
    MethodHandle* methodHandle;
    bool autorelease;
    ffi_closure* ffiClosure;
    VALUE rbProc;
    VALUE rbFunctionInfo;
} Function;

static void function_mark(Function *);
static void function_free(Function *);
static VALUE function_init(VALUE self, VALUE rbFunctionInfo, VALUE rbProc);
static void callback_invoke(ffi_cif* cif, void* retval, void** parameters, void* user_data);

VALUE rbffi_FunctionClass = Qnil;

static ID id_call = 0, id_cbtable = 0;

static VALUE
function_allocate(VALUE klass)
{
    Function *fn;
    VALUE obj;

    obj = Data_Make_Struct(klass, Function, function_mark, function_free, fn);

    fn->memory.access = MEM_RD;
    fn->memory.ops = &rbffi_AbstractMemoryOps;

    fn->rbProc = Qnil;
    fn->rbFunctionInfo = Qnil;
    fn->autorelease = true;

    return obj;
}

static void
function_mark(Function *fn)
{
    rb_gc_mark(fn->rbProc);
    rb_gc_mark(fn->rbFunctionInfo);
}

static void
function_free(Function *fn)
{
    if (fn->methodHandle != NULL) {
        rbffi_MethodHandle_Free(fn->methodHandle);
    }

    if (fn->ffiClosure != NULL && fn->autorelease) {
        ffi_closure_free(fn->ffiClosure);
    }

    xfree(fn);
}

static VALUE
function_initialize(int argc, VALUE* argv, VALUE self)
{
    
    VALUE rbReturnType = Qnil, rbParamTypes = Qnil, rbProc = Qnil, rbOptions = Qnil;
    VALUE rbFunctionInfo = Qnil;
    VALUE infoArgv[3];
    int nargs;

    nargs = rb_scan_args(argc, argv, "22", &rbReturnType, &rbParamTypes, &rbProc, &rbOptions);

    //
    // Callback with block,
    // e.g. Function.new(:int, [ :int ]) { |i| blah }
    // or   Function.new(:int, [ :int ], { :convention => :stdcall }) { |i| blah }
    //
    if (rb_block_given_p()) {
        if (nargs > 3) {
            rb_raise(rb_eArgError, "cannot create function with both proc/address and block");
        }
        rbOptions = rbProc;
        rbProc = rb_block_proc();
    } else {
        // Callback with proc, or Function with address
        // e.g. Function.new(:int, [ :int ], Proc.new { |i| })
        //      Function.new(:int, [ :int ], Proc.new { |i| }, { :convention => :stdcall })
        //      Function.new(:int, [ :int ], addr)
        //      Function.new(:int, [ :int ], addr, { :convention => :stdcall })
    }
    
    infoArgv[0] = rbReturnType;
    infoArgv[1] = rbParamTypes;
    infoArgv[2] = rbOptions;
    rbFunctionInfo = rb_class_new_instance(rbOptions != Qnil ? 3 : 2, infoArgv, rbffi_FunctionTypeClass);

    function_init(self, rbFunctionInfo, rbProc);
    
    return self;
}

VALUE
rbffi_Function_NewInstance(VALUE rbFunctionInfo, VALUE rbProc)
{
    return function_init(function_allocate(rbffi_FunctionClass), rbFunctionInfo, rbProc);
}

VALUE
rbffi_Function_ForProc(VALUE rbFunctionInfo, VALUE proc)
{
    VALUE callback;
    VALUE cbTable = RTEST(rb_ivar_defined(proc, id_cbtable)) ? rb_ivar_get(proc, id_cbtable) : Qnil;

    if (cbTable == Qnil) {
        cbTable = rb_hash_new();
        rb_ivar_set(proc, id_cbtable, cbTable);
    }

    callback = rb_hash_aref(cbTable, rbFunctionInfo);
    if (callback != Qnil) {
        return callback;
    }

    callback = rbffi_Function_NewInstance(rbFunctionInfo, proc);
    rb_hash_aset(cbTable, rbFunctionInfo, callback);
    return callback;
}

static VALUE
function_init(VALUE self, VALUE rbFunctionInfo, VALUE rbProc)
{
    Function* fn = NULL;
    
    Data_Get_Struct(self, Function, fn);

    fn->rbFunctionInfo = rbFunctionInfo;

    Data_Get_Struct(fn->rbFunctionInfo, FunctionType, fn->info);

    if (rb_obj_is_kind_of(rbProc, rbffi_PointerClass)) {
        AbstractMemory* memory;
        Data_Get_Struct(rbProc, AbstractMemory, memory);
        fn->memory = *memory;

    } else if (rb_obj_is_kind_of(rbProc, rb_cProc) || rb_respond_to(rbProc, id_call)) {
        void* code;
        ffi_status status;
        fn->ffiClosure = ffi_closure_alloc(sizeof(*fn->ffiClosure), &code);
        if (fn->ffiClosure == NULL) {
            rb_raise(rb_eNoMemError, "Failed to allocate libffi closure");
        }

        status = ffi_prep_closure_loc(fn->ffiClosure, &fn->info->ffi_cif,
                callback_invoke, fn, code);
        if (status != FFI_OK) {
            rb_raise(rb_eArgError, "ffi_prep_closure_loc failed");
        }

        fn->memory.address = code;
        fn->memory.size = sizeof(*fn->ffiClosure);
        fn->autorelease = true;

    } else {
        rb_raise(rb_eTypeError, "wrong argument type.  Expected pointer or proc");
    }
    
    fn->rbProc = rbProc;

    return self;
}

static VALUE
function_call(int argc, VALUE* argv, VALUE self)
{
    Function* fn;

    Data_Get_Struct(self, Function, fn);

    return (*fn->info->invoke)(argc, argv, fn->memory.address, fn->info);
}

static VALUE
function_attach(VALUE self, VALUE module, VALUE name)
{
    Function* fn;
    char var[1024];

    Data_Get_Struct(self, Function, fn);

    if (fn->info->parameterCount == -1) {
        rb_raise(rb_eRuntimeError, "Cannot attach variadic functions");
    }

    if (fn->methodHandle == NULL) {
        fn->methodHandle = rbffi_MethodHandle_Alloc(fn->info, fn->memory.address);
    }

    //
    // Stash the Function in a module variable so it does not get garbage collected
    //
    snprintf(var, sizeof(var), "@@%s", StringValueCStr(name));
    rb_cv_set(module, var, self);

    rb_define_module_function(module, StringValueCStr(name),
            rbffi_MethodHandle_CodeAddress(fn->methodHandle), -1);

    
    rb_define_method(module, StringValueCStr(name),
            rbffi_MethodHandle_CodeAddress(fn->methodHandle), -1);

    return self;
}

static VALUE
function_set_autorelease(VALUE self, VALUE autorelease)
{
    Function* fn;

    Data_Get_Struct(self, Function, fn);

    fn->autorelease = RTEST(autorelease);

    return self;
}

static VALUE
function_autorelease_p(VALUE self)
{
    Function* fn;

    Data_Get_Struct(self, Function, fn);

    return fn->autorelease ? Qtrue : Qfalse;
}

static VALUE
function_release(VALUE self)
{
    Function* fn;

    Data_Get_Struct(self, Function, fn);

    if (fn->ffiClosure == NULL) {
        rb_raise(rb_eRuntimeError, "cannot free function which was not allocated");
    }
    
    ffi_closure_free(fn->ffiClosure);
    fn->ffiClosure = NULL;
    
    return self;
}

static void
callback_invoke(ffi_cif* cif, void* retval, void** parameters, void* user_data)
{
    Function* fn = (Function *) user_data;
    FunctionType *cbInfo = fn->info;
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
                param = (*(void **) parameters[i] != NULL) ? rb_tainted_str_new2(*(char **) parameters[i]) : Qnil;
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
    rbReturnValue = rb_funcall2(fn->rbProc, id_call, cbInfo->parameterCount, rbParams);
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
                VALUE function;

                function = rbffi_Function_ForProc(cbInfo->rbReturnType, rbReturnValue);

                *((void **) retval) = ((AbstractMemory *) DATA_PTR(function))->address;
            } else {
                *((void **) retval) = NULL;
            }
            break;

        default:
            *((ffi_arg *) retval) = 0;
            break;
    }
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
rbffi_Function_Init(VALUE moduleFFI)
{
    rbffi_FunctionInfo_Init(moduleFFI);
    rbffi_FunctionClass = rb_define_class_under(moduleFFI, "Function", rbffi_PointerClass);
    
    rb_global_variable(&rbffi_FunctionClass);
    rb_define_alloc_func(rbffi_FunctionClass, function_allocate);

    rb_define_method(rbffi_FunctionClass, "initialize", function_initialize, -1);
    rb_define_method(rbffi_FunctionClass, "call", function_call, -1);
    rb_define_method(rbffi_FunctionClass, "attach", function_attach, 2);
    rb_define_method(rbffi_FunctionClass, "free", function_release, 0);
    rb_define_method(rbffi_FunctionClass, "autorelease=", function_set_autorelease, 1);
    rb_define_method(rbffi_FunctionClass, "autorelease", function_autorelease_p, 0);
    rb_define_method(rbffi_FunctionClass, "autorelease?", function_autorelease_p, 0);

    id_call = rb_intern("call");
    id_cbtable = rb_intern("@__ffi_callback_table__");
}

