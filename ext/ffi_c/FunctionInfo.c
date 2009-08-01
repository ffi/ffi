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
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <ruby.h>

#include <ffi.h>
#include "rbffi.h"
#include "compat.h"

#include "AbstractMemory.h"
#include "Types.h"
#include "Type.h"
#include "Function.h"


static void fninfo_mark(FunctionInfo*);
static void fninfo_free(FunctionInfo *);

VALUE rbffi_FunctionInfoClass = Qnil;

static VALUE
fninfo_allocate(VALUE klass)
{
    FunctionInfo* fnInfo;
    VALUE obj = Data_Make_Struct(klass, FunctionInfo, fninfo_mark, fninfo_free, fnInfo);

    fnInfo->type.ffiType = &ffi_type_pointer;
    fnInfo->type.size = ffi_type_pointer.size;
    fnInfo->type.alignment = ffi_type_pointer.alignment;
    fnInfo->type.nativeType = NATIVE_FUNCTION;
    fnInfo->rbReturnType = Qnil;
    fnInfo->rbParameterTypes = Qnil;
    fnInfo->rbEnums = Qnil;

    return obj;
}

static void
fninfo_mark(FunctionInfo* fnInfo)
{
    rb_gc_mark(fnInfo->rbReturnType);
    rb_gc_mark(fnInfo->rbParameterTypes);
    rb_gc_mark(fnInfo->rbEnums);
    if (fnInfo->callbackCount > 0 && fnInfo->callbackParameters != NULL) {
        rb_gc_mark_locations(&fnInfo->callbackParameters[0], &fnInfo->callbackParameters[fnInfo->callbackCount]);
    }
}

static void
fninfo_free(FunctionInfo* fnInfo)
{
    xfree(fnInfo->parameterTypes);
    xfree(fnInfo->ffiParameterTypes);
    xfree(fnInfo->nativeParameterTypes);
    xfree(fnInfo->callbackParameters);
    xfree(fnInfo);
}

static VALUE
fninfo_initialize(int argc, VALUE* argv, VALUE self)
{
    FunctionInfo *fnInfo;
    ffi_status status;
    VALUE rbReturnType = Qnil, rbParamTypes = Qnil, rbOptions = Qnil;
    VALUE rbEnums = Qnil, rbConvention = Qnil;
    int i, nargs;

    nargs = rb_scan_args(argc, argv, "21", &rbReturnType, &rbParamTypes, &rbOptions);
    if (nargs >= 3 && rbOptions != Qnil) {
        rbConvention = rb_hash_aref(rbOptions, ID2SYM(rb_intern("convention")));
        rbEnums = rb_hash_aref(rbOptions, ID2SYM(rb_intern("enums")));
    }

    Check_Type(rbParamTypes, T_ARRAY);

    Data_Get_Struct(self, FunctionInfo, fnInfo);
    fnInfo->parameterCount = RARRAY_LEN(rbParamTypes);
    fnInfo->parameterTypes = xcalloc(fnInfo->parameterCount, sizeof(*fnInfo->parameterTypes));
    fnInfo->ffiParameterTypes = xcalloc(fnInfo->parameterCount, sizeof(ffi_type *));
    fnInfo->nativeParameterTypes = xcalloc(fnInfo->parameterCount, sizeof(*fnInfo->nativeParameterTypes));
    fnInfo->rbParameterTypes = rb_ary_new2(fnInfo->parameterCount);
    fnInfo->rbEnums = rbEnums;

    for (i = 0; i < fnInfo->parameterCount; ++i) {
        VALUE entry = rb_ary_entry(rbParamTypes, i);
        VALUE type = rbffi_Type_Lookup(entry);

        if (!RTEST(type)) {
            VALUE typeName = rb_funcall2(entry, rb_intern("inspect"), 0, NULL);
            rb_raise(rb_eTypeError, "Invalid parameter type (%s)", RSTRING_PTR(typeName));
        }

        if (rb_obj_is_kind_of(type, rbffi_FunctionInfoClass)) {
            REALLOC_N(fnInfo->callbackParameters, VALUE, fnInfo->callbackCount + 1);
            fnInfo->callbackParameters[fnInfo->callbackCount++] = type;
        }

        rb_ary_push(fnInfo->rbParameterTypes, type);
        Data_Get_Struct(type, Type, fnInfo->parameterTypes[i]);
        fnInfo->ffiParameterTypes[i] = fnInfo->parameterTypes[i]->ffiType;
        fnInfo->nativeParameterTypes[i] = fnInfo->parameterTypes[i]->nativeType;
    }

    fnInfo->rbReturnType = rbffi_Type_Lookup(rbReturnType);
    if (!RTEST(fnInfo->rbReturnType)) {
        VALUE typeName = rb_funcall2(rbReturnType, rb_intern("inspect"), 0, NULL);
        rb_raise(rb_eTypeError, "Invalid return type (%s)", RSTRING_PTR(typeName));
    }

    Data_Get_Struct(fnInfo->rbReturnType, Type, fnInfo->returnType);
    fnInfo->ffiReturnType = fnInfo->returnType->ffiType;


#if defined(_WIN32) || defined(__WIN32__)
    fnInfo->abi = (rbConvention != Qnil && strcmp(StringValueCStr(rbConvention), "stdcall") == 0)
            ? FFI_STDCALL : FFI_DEFAULT_ABI;
#else
    fnInfo->abi = FFI_DEFAULT_ABI;
#endif

    status = ffi_prep_cif(&fnInfo->ffi_cif, fnInfo->abi, fnInfo->parameterCount,
            fnInfo->ffiReturnType, fnInfo->ffiParameterTypes);
    switch (status) {
        case FFI_BAD_ABI:
            rb_raise(rb_eArgError, "Invalid ABI specified");
        case FFI_BAD_TYPEDEF:
            rb_raise(rb_eArgError, "Invalid argument type specified");
        case FFI_OK:
            break;
        default:
            rb_raise(rb_eArgError, "Unknown FFI error");
    }

    return self;
}

void
rbffi_FunctionInfo_Init(VALUE moduleFFI)
{
    rbffi_FunctionInfoClass = rb_define_class_under(moduleFFI, "FunctionInfo", rbffi_TypeClass);
    rb_global_variable(&rbffi_FunctionInfoClass);
    rb_define_const(moduleFFI, "CallbackInfo", rbffi_FunctionInfoClass);

    rb_define_alloc_func(rbffi_FunctionInfoClass, fninfo_allocate);
    rb_define_method(rbffi_FunctionInfoClass, "initialize", fninfo_initialize, -1);

}

