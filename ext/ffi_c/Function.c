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
#include <errno.h>
#include <ruby.h>
#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32) && !defined(__WIN32__)
# include <pthread.h>
#endif

#include <ffi.h>
#include "rbffi.h"
#include "compat.h"

#include "AbstractMemory.h"
#include "Pointer.h"
#include "Struct.h"
#include "Platform.h"
#include "Callback.h"
#include "Types.h"
#include "Type.h"
#include "LastError.h"
#include "Function.h"

typedef struct Function_ {
    AbstractMemory memory;
    FunctionInfo* info;
    void* address;
    bool autorelease;
    bool allocated;

    VALUE rbAddress;
    VALUE rbFunctionInfo;
    VALUE rbInvoker;
} Function;

static void function_mark(Function *);
static void function_free(Function *);

VALUE rbffi_FunctionClass;

static VALUE
function_allocate(VALUE klass)
{
    Function *fn;
    VALUE obj;

    obj = Data_Make_Struct(klass, Function, function_mark, function_free, fn);

    fn->rbAddress = Qnil;
    fn->rbFunctionInfo = Qnil;
    fn->rbInvoker = Qnil;
    fn->autorelease = true;
    fn->allocated = false;

    return obj;
}

static void
function_mark(Function *fn)
{
    rb_gc_mark(fn->rbAddress);
    rb_gc_mark(fn->rbFunctionInfo);
    rb_gc_mark(fn->rbInvoker);
}

static void
function_free(Function *fn)
{

}

static VALUE
function_initialize(int argc, VALUE* argv, VALUE self)
{
    Function* fn = NULL;
    VALUE rbReturnType = Qnil, rbParamTypes = Qnil, rbProc = Qnil, rbOptions = Qnil;
    VALUE infoArgv[3], invokerArgv[6];
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
    
    if (!rb_obj_is_kind_of(rbProc, rbffi_PointerClass) && !rb_obj_is_kind_of(rbProc, rb_cProc)) {
        rb_raise(rb_eTypeError, "wrong argument type.  Expected pointer or proc");
    }

    Data_Get_Struct(self, Function, fn);

    infoArgv[0] = rbReturnType;
    infoArgv[1] = rbParamTypes;
    infoArgv[2] = rbOptions;
    fn->rbFunctionInfo = rb_class_new_instance(rbOptions != Qnil ? 3 : 2, infoArgv, rbffi_FunctionInfoClass);

    Data_Get_Struct(fn->rbFunctionInfo, FunctionInfo, fn->info);
    if (rb_obj_is_kind_of(rbProc, rbffi_PointerClass)) {
        AbstractMemory* memory;
        Data_Get_Struct(rbProc, AbstractMemory, memory);
        fn->memory = *memory;
        fn->rbAddress = rbProc;

    } else if (rb_obj_is_kind_of(rbProc, rb_cProc)) {
        
        fn->rbAddress = rbffi_NativeCallback_NewInstance(fn->rbFunctionInfo, rbProc);
        NativeCallback* cb;
        Data_Get_Struct(fn->rbAddress, NativeCallback, cb);
        fn->memory.address = cb->code;
        fn->memory.size = LONG_MAX;
        fn->allocated = true;

    } else {
        rb_raise(rb_eTypeError, "wrong argument type.  Expected pointer or proc");
    }

    //
    // For now, just delegate to a standard invoker instance, until it is refactored
    //
    invokerArgv[0] = self;
    invokerArgv[1] = fn->info->rbParameterTypes;
    invokerArgv[2] = fn->info->rbReturnType;
    invokerArgv[3] = Qnil;
    invokerArgv[4] = Qnil;
    if (rb_obj_is_kind_of(rbOptions, rb_cHash)) {
        invokerArgv[3] = rb_hash_aref(rbOptions, ID2SYM(rb_intern("convention")));
        invokerArgv[4] = rb_hash_aref(rbOptions, ID2SYM(rb_intern("enums")));
    }
    if (!RTEST(invokerArgv[3])) {
        invokerArgv[3] = rb_str_new2("default");
    }

    fn->rbInvoker = rb_class_new_instance(5, invokerArgv, rbffi_InvokerClass);

    return self;
}

static VALUE
function_call(int argc, VALUE* argv, VALUE self)
{
    Function* fn;

    Data_Get_Struct(self, Function, fn);
    
    return rb_funcall2(fn->rbInvoker, rb_intern("call"), argc, argv);
}

static VALUE
function_attach(int argc, VALUE* argv, VALUE self)
{
    Function* fn;

    Data_Get_Struct(self, Function, fn);

    return rb_funcall2(fn->rbInvoker, rb_intern("attach"), argc, argv);
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
function_autorelease_p(VALUE self, VALUE autorelease)
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

    if (!fn->allocated) {
        rb_raise(rb_eRuntimeError, "cannot free function which was not allocated");
    }

    return self;
}

void
rbffi_Function_Init(VALUE moduleFFI)
{
    rbffi_FunctionInfo_Init(moduleFFI);
    rbffi_FunctionClass = rb_define_class_under(moduleFFI, "Function", rbffi_PointerClass);
    
    rb_global_variable(&rbffi_FunctionClass);
    rb_define_alloc_func(rbffi_FunctionClass, function_allocate);

    rb_define_method(rbffi_FunctionClass, "initialize", function_initialize, -1);
    rb_define_method(rbffi_FunctionClass, "call", function_call, -1);
    rb_define_method(rbffi_FunctionClass, "attach", function_attach, -1);
    rb_define_method(rbffi_FunctionClass, "free", function_release, 0);
    rb_define_method(rbffi_FunctionClass, "autorelease=", function_set_autorelease, 1);
    rb_define_method(rbffi_FunctionClass, "autorelease", function_autorelease_p, 0);
}

