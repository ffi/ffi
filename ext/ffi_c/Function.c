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
#include "Pointer.h"
#include "Struct.h"
#include "Platform.h"
#include "Callback.h"
#include "Types.h"
#include "Type.h"
#include "LastError.h"
#include "Function.h"

VALUE rbffi_FunctionClass;

typedef struct Function_ {
    AbstractMemory memory;
    FunctionInfo* info;
    void* address;

    VALUE rbAddress;
    VALUE enums;
    VALUE rbReturnType;
    VALUE callbackArray;
    int callbackCount;
    VALUE* callbackParameters;
    ffi_abi abi;
} Function;

static VALUE
function_allocate(VALUE klass)
{
#ifdef notyet
    Function *function;
    VALUE obj = Data_Make_Struct(klass, Function, function_mark, function_free, function);

    function->rbAddress = Qnil;
    function->rbReturnType = Qnil;
    function->callbackArray = Qnil;
    function->address = Qnil;
    function->enums = Qnil;

    return obj;
#else
    return Qnil;
#endif
}

void
rbffi_Function_Init(VALUE moduleFFI)
{
    rbffi_FunctionInfo_Init(moduleFFI);
    rbffi_FunctionClass = rb_define_class_under(moduleFFI, "Function", rbffi_PointerClass);
    
    rb_global_variable(&rbffi_FunctionClass);
    rb_define_alloc_func(rbffi_FunctionClass, function_allocate);
#ifdef notyet
    rb_define_method(rbffi_FunctionClass, "initialize", function_initialize, 6);
    rb_define_method(rbffi_FunctionClass, "call", function_call, -1);
#endif
}

