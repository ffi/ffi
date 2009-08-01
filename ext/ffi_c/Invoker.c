/*
 * Copyright (c) 2008, 2009, Wayne Meissner
 * Copyright (c) 2009, Luc Heinrich <luc@honk-honk.com>
 * Copyright (c) 2009, Mike Dalessio <mike.dalessio@gmail.com>
 * Copyright (c) 2009, Aman Gupta.
 * 
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
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#ifndef _WIN32
#  include <unistd.h>
#endif
#include <errno.h>
#include <ruby.h>

#include <ffi.h>
#include "rbffi.h"
#include "compat.h"

#include "AbstractMemory.h"
#include "Pointer.h"
#include "Function.h"
#include "Callback.h"
#include "Types.h"
#include "Type.h"
#include "LastError.h"
#include "MethodHandle.h"
#include "Call.h"


#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32) && !defined(__WIN32__)
#  define USE_PTHREAD_LOCAL
#endif


#ifndef roundup
#  define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif

typedef struct Invoker_ {
    VALUE rbFunction;
    VALUE rbFunctionInfo;
    
    void* function;
    MethodHandle* methodHandle;
    FunctionInfo* fnInfo;
} Invoker;

static VALUE invoker_allocate(VALUE klass);
static VALUE invoker_initialize(VALUE self, VALUE function, VALUE parameterTypes,
        VALUE returnType, VALUE convention, VALUE enums);
static void invoker_mark(Invoker *);
static void invoker_free(Invoker *);
static VALUE invoker_call(int argc, VALUE* argv, VALUE self);
static 
VALUE rbffi_InvokerClass = Qnil;
static VALUE classInvoker = Qnil;

static VALUE
invoker_allocate(VALUE klass)
{
    Invoker *invoker;
    VALUE obj = Data_Make_Struct(klass, Invoker, invoker_mark, invoker_free, invoker);

    invoker->rbFunctionInfo = Qnil;
    invoker->rbFunction = Qnil;
    
    return obj;
}

static VALUE
invoker_initialize(VALUE self, VALUE rbFunction, VALUE rbParamTypes,
       VALUE rbReturnType, VALUE convention, VALUE enums)
{
    Invoker* invoker = NULL;
    VALUE rbOptions = Qnil;
    VALUE infoArgv[3];
    
    Data_Get_Struct(self, Invoker, invoker);

    invoker->function = rbffi_AbstractMemory_Cast(rbFunction, rbffi_PointerClass)->address;
    invoker->rbFunction = rbFunction;

    infoArgv[0] = rbReturnType;
    infoArgv[1] = rbParamTypes;
    infoArgv[2] = rbOptions = rb_hash_new();
    rb_hash_aset(rbOptions, ID2SYM(rb_intern("enums")), enums);
    rb_hash_aset(rbOptions, ID2SYM(rb_intern("convention")), convention);
    invoker->rbFunctionInfo = rb_class_new_instance(3, infoArgv, rbffi_FunctionInfoClass);
    Data_Get_Struct(invoker->rbFunctionInfo, FunctionInfo, invoker->fnInfo);
    
    invoker->methodHandle = rbffi_MethodHandle_Alloc(invoker->fnInfo, invoker->function);
    
    return self;
}

static inline VALUE
ffi_invoke(ffi_cif* cif, void* function, Type* returnType, void** ffiValues,
    VALUE rbReturnType, VALUE rbEnums)
{
    FFIStorage retval;

#ifdef USE_RAW
    ffi_raw_call(cif, function, &retval, (ffi_raw *) ffiValues[0]);
#else
    ffi_call(cif, function, &retval, ffiValues);
#endif
    rbffi_save_errno();

    return rbffi_NativeValue_ToRuby(returnType, rbReturnType, &retval, rbEnums);
}

static VALUE
invoker_call(int argc, VALUE* argv, VALUE self)
{
    Invoker* invoker;
    
    Data_Get_Struct(self, Invoker, invoker);

    return rbffi_CallFunction(argc, argv, invoker->function, invoker->fnInfo);
}


static VALUE
invoker_attach(VALUE self, VALUE module, VALUE name)
{
    Invoker* invoker;
    MethodHandle* handle;
    char var[1024];

    Data_Get_Struct(self, Invoker, invoker);

    snprintf(var, sizeof(var), "@@%s", StringValueCStr(name));
    rb_cv_set(module, var, self);

    handle = invoker->methodHandle;
    rb_define_module_function(module, StringValueCStr(name),
        rbffi_MethodHandle_CodeAddress(invoker->methodHandle), -1);
    
    rb_define_method(module, StringValueCStr(name),
        rbffi_MethodHandle_CodeAddress(invoker->methodHandle), -1);

    return self;
}

static VALUE
invoker_arity(VALUE self)
{
    Invoker* invoker;
    Data_Get_Struct(self, Invoker, invoker);
    return INT2FIX(invoker->fnInfo->parameterCount);
}

static void
invoker_mark(Invoker *invoker)
{
    rb_gc_mark(invoker->rbFunctionInfo);
    rb_gc_mark(invoker->rbFunction);
}

static void
invoker_free(Invoker *invoker)
{
    rbffi_MethodHandle_Free(invoker->methodHandle);
    xfree(invoker);
}

void 
rbffi_Invoker_Init(VALUE moduleFFI)
{
    
    rbffi_InvokerClass = classInvoker = rb_define_class_under(moduleFFI, "Invoker", rb_cObject);
    rb_global_variable(&rbffi_InvokerClass);
    rb_global_variable(&classInvoker);

    rb_define_alloc_func(classInvoker, invoker_allocate);
    rb_define_method(classInvoker, "initialize", invoker_initialize, 5);
    rb_define_method(classInvoker, "call", invoker_call, -1);
    rb_define_method(classInvoker, "arity", invoker_arity, 0);
    rb_define_method(classInvoker, "attach", invoker_attach, 2);
}
