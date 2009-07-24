#include <sys/param.h>
#include <sys/types.h>
#ifndef _WIN32
#  include <sys/mman.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <ruby.h>
#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32) && !defined(__WIN32__)
# include <pthread.h>
#endif

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
    fnInfo->rbParameterTypes = NULL;

    return obj;
}

static void
fninfo_mark(FunctionInfo* fnInfo)
{
    rb_gc_mark(fnInfo->rbReturnType);
    rb_gc_mark(fnInfo->rbParameterTypes);
}

static void
fninfo_free(FunctionInfo* fnInfo)
{
    if (fnInfo->parameterTypes != NULL) {
        xfree(fnInfo->parameterTypes);
    }
    if (fnInfo->ffiParameterTypes != NULL) {
        xfree(fnInfo->ffiParameterTypes);
    }
    xfree(fnInfo);
}

static VALUE
fninfo_initialize(int argc, VALUE* argv, VALUE self)
{
    FunctionInfo *fnInfo;
    ffi_status status;
    VALUE rbReturnType = Qnil, rbParamTypes = Qnil, rbOptions = Qnil;
    int i, nargs;

    nargs = rb_scan_args(argc, argv, "21", &rbReturnType, &rbParamTypes, &rbOptions);
    
    Check_Type(rbParamTypes, T_ARRAY);

    Data_Get_Struct(self, FunctionInfo, fnInfo);
    fnInfo->parameterCount = RARRAY_LEN(rbParamTypes);
    fnInfo->parameterTypes = xcalloc(fnInfo->parameterCount, sizeof(*fnInfo->parameterTypes));
    fnInfo->ffiParameterTypes = xcalloc(fnInfo->parameterCount, sizeof(ffi_type *));
    fnInfo->rbParameterTypes = rb_ary_new2(fnInfo->parameterCount);

    for (i = 0; i < fnInfo->parameterCount; ++i) {
        VALUE entry = rb_ary_entry(rbParamTypes, i);
        VALUE type = rbffi_Type_Lookup(entry);

        if (!RTEST(type)) {
            VALUE typeName = rb_funcall2(entry, rb_intern("inspect"), 0, NULL);
            rb_raise(rb_eTypeError, "Invalid parameter type (%s)", RSTRING_PTR(typeName));
        }

        rb_ary_push(fnInfo->rbParameterTypes, type);
        Data_Get_Struct(type, Type, fnInfo->parameterTypes[i]);
        fnInfo->ffiParameterTypes[i] = fnInfo->parameterTypes[i]->ffiType;
    }

    fnInfo->rbReturnType = rbffi_Type_Lookup(rbReturnType);
    if (!RTEST(fnInfo->rbReturnType)) {
        VALUE typeName = rb_funcall2(rbReturnType, rb_intern("inspect"), 0, NULL);
        rb_raise(rb_eTypeError, "Invalid return type (%s)", RSTRING_PTR(typeName));
    }

    Data_Get_Struct(fnInfo->rbReturnType, Type, fnInfo->returnType);
    fnInfo->ffiReturnType = fnInfo->returnType->ffiType;

#if defined(_WIN32) && defined(notyet)
    fnInfo->abi = (flags & STDCALL) ? FFI_STDCALL : FFI_DEFAULT_ABI;
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

