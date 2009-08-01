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
#  include <pthread.h>
#endif
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

typedef struct VariadicInvoker_ {
    VALUE rbAddress;
    VALUE rbReturnType;
    VALUE rbEnums;

    Type* returnType;
    ffi_abi abi;
    void* function;
    int paramCount;
} VariadicInvoker;


static VALUE invoker_allocate(VALUE klass);
static VALUE invoker_initialize(VALUE self, VALUE function, VALUE parameterTypes,
        VALUE returnType, VALUE convention, VALUE enums);
static void invoker_mark(Invoker *);
static void invoker_free(Invoker *);
static VALUE invoker_call(int argc, VALUE* argv, VALUE self);
static VALUE invoker_arity(VALUE self);

static VALUE variadic_allocate(VALUE klass);
static VALUE variadic_initialize(VALUE self, VALUE rbFunction, VALUE rbParameterTypes,
        VALUE rbReturnType, VALUE options);
static void variadic_mark(VariadicInvoker *);
static void variadic_free(VariadicInvoker *);

VALUE rbffi_InvokerClass = Qnil;
static VALUE classInvoker = Qnil, classVariadicInvoker = Qnil;

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

static VALUE
variadic_allocate(VALUE klass)
{
    VariadicInvoker *invoker;
    VALUE obj = Data_Make_Struct(klass, VariadicInvoker, variadic_mark, variadic_free, invoker);

    invoker->rbAddress = Qnil;
    invoker->rbEnums = Qnil;
    invoker->rbReturnType = Qnil;

    return obj;
}

static VALUE
variadic_initialize(VALUE self, VALUE rbFunction, VALUE rbParameterTypes, VALUE rbReturnType, VALUE options)
{
    VariadicInvoker* invoker = NULL;
    VALUE retval = Qnil;
    VALUE convention = Qnil;
    VALUE fixed = Qnil;
    int i;

    Check_Type(options, T_HASH);
    convention = rb_hash_aref(options, ID2SYM(rb_intern("convention")));

    Data_Get_Struct(self, VariadicInvoker, invoker);
    invoker->rbEnums = rb_hash_aref(options, ID2SYM(rb_intern("enums")));
    invoker->rbAddress = rbFunction;
    invoker->function = rbffi_AbstractMemory_Cast(rbFunction, rbffi_PointerClass)->address;

#if defined(_WIN32) || defined(__WIN32__)
    invoker->abi = (RTEST(convention) && strcmp(StringValueCStr(convention), "stdcall") == 0)
            ? FFI_STDCALL : FFI_DEFAULT_ABI;
#else
    invoker->abi = FFI_DEFAULT_ABI;
#endif

    invoker->rbReturnType = rbffi_Type_Lookup(rbReturnType);
    if (!RTEST(invoker->rbReturnType)) {
        VALUE typeName = rb_funcall2(rbReturnType, rb_intern("inspect"), 0, NULL);
        rb_raise(rb_eTypeError, "Invalid return type (%s)", RSTRING_PTR(typeName));
    }

    Data_Get_Struct(rbReturnType, Type, invoker->returnType);

    invoker->paramCount = -1;

    fixed = rb_ary_new2(RARRAY_LEN(rbParameterTypes) - 1);
    for (i = 0; i < RARRAY_LEN(rbParameterTypes); ++i) {
        VALUE entry = rb_ary_entry(rbParameterTypes, i);
        VALUE rbType = rbffi_Type_Lookup(entry);
        Type* type;

        if (!RTEST(rbType)) {
            VALUE typeName = rb_funcall2(entry, rb_intern("inspect"), 0, NULL);
            rb_raise(rb_eTypeError, "Invalid parameter type (%s)", RSTRING_PTR(typeName));
        }
        Data_Get_Struct(rbType, Type, type);
        if (type->nativeType != NATIVE_VARARGS) {
            rb_ary_push(fixed, entry);
        }
    }
    /*
     * @fixed and @type_map are used by the parameter mangling ruby code
     */
    rb_iv_set(self, "@fixed", fixed);
    rb_iv_set(self, "@type_map", rb_hash_aref(options, ID2SYM(rb_intern("type_map"))));

    return retval;
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
    FFIStorage* params = NULL;
    void** ffiValues = NULL;

    Data_Get_Struct(self, Invoker, invoker);

    params = ALLOCA_N(FFIStorage, invoker->fnInfo->parameterCount);
    ffiValues = ALLOCA_N(void *, invoker->fnInfo->parameterCount);
    
    rbffi_SetupCallParams(argc, argv, invoker->fnInfo->parameterCount, invoker->fnInfo->nativeParameterTypes,
        params, ffiValues,
        invoker->fnInfo->callbackParameters, invoker->fnInfo->callbackCount, invoker->fnInfo->rbEnums);

    return ffi_invoke(&invoker->fnInfo->ffi_cif, invoker->function, invoker->fnInfo->returnType,
        ffiValues, invoker->fnInfo->rbReturnType, invoker->fnInfo->rbEnums);
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

static VALUE
variadic_call(VALUE self, VALUE parameterTypes, VALUE parameterValues)
{
    VariadicInvoker* invoker;
    FFIStorage* params;
    ffi_cif cif;
    void** ffiValues;
    ffi_type** ffiParamTypes;
    ffi_type* ffiReturnType;
    NativeType* paramTypes;
    VALUE* argv;
    int paramCount = 0, i;
    ffi_status ffiStatus;

    Check_Type(parameterTypes, T_ARRAY);
    Check_Type(parameterValues, T_ARRAY);

    Data_Get_Struct(self, VariadicInvoker, invoker);
    paramCount = RARRAY_LEN(parameterTypes);
    paramTypes = ALLOCA_N(NativeType, paramCount);
    ffiParamTypes = ALLOCA_N(ffi_type *, paramCount);
    params = ALLOCA_N(FFIStorage, paramCount);
    ffiValues = ALLOCA_N(void*, paramCount);
    argv = ALLOCA_N(VALUE, paramCount);

    for (i = 0; i < paramCount; ++i) {
        VALUE entry = rb_ary_entry(parameterTypes, i);
        int paramType = rbffi_Type_GetIntValue(entry);
        Type* type;
        Data_Get_Struct(entry, Type, type);

        switch (paramType) {
            case NATIVE_INT8:
            case NATIVE_INT16:
            case NATIVE_INT32:
            case NATIVE_ENUM:
                paramType = NATIVE_INT32;
                ffiParamTypes[i] = &ffi_type_sint;
                break;
            case NATIVE_UINT8:
            case NATIVE_UINT16:
            case NATIVE_UINT32:
                paramType = NATIVE_UINT32;
                ffiParamTypes[i] = &ffi_type_uint;
                break;
            case NATIVE_FLOAT32:
                paramType = NATIVE_FLOAT64;
                ffiParamTypes[i] = &ffi_type_double;
                break;
            default:
                ffiParamTypes[i] = type->ffiType;
                break;
        }
        paramTypes[i] = paramType;
        if (ffiParamTypes[i] == NULL) {
            rb_raise(rb_eArgError, "Invalid parameter type #%x", paramType);
        }
        argv[i] = rb_ary_entry(parameterValues, i);
    }

    ffiReturnType = invoker->returnType->ffiType;
    if (ffiReturnType == NULL) {
        rb_raise(rb_eArgError, "Invalid return type");
    }
    ffiStatus = ffi_prep_cif(&cif, invoker->abi, paramCount, ffiReturnType, ffiParamTypes);
    switch (ffiStatus) {
        case FFI_BAD_ABI:
            rb_raise(rb_eArgError, "Invalid ABI specified");
        case FFI_BAD_TYPEDEF:
            rb_raise(rb_eArgError, "Invalid argument type specified");
        case FFI_OK:
            break;
        default:
            rb_raise(rb_eArgError, "Unknown FFI error");
    }

    rbffi_SetupCallParams(paramCount, argv, -1, paramTypes, params,
        ffiValues, NULL, 0, invoker->rbEnums);

    return ffi_invoke(&cif, invoker->function, invoker->returnType,
        ffiValues, invoker->rbReturnType, invoker->rbEnums);
}

static void
variadic_mark(VariadicInvoker *invoker)
{
    rb_gc_mark(invoker->rbEnums);
    rb_gc_mark(invoker->rbAddress);
    rb_gc_mark(invoker->rbReturnType);
}

static void
variadic_free(VariadicInvoker *invoker)
{
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
    classVariadicInvoker = rb_define_class_under(moduleFFI, "VariadicInvoker", rb_cObject);
    rb_global_variable(&classVariadicInvoker);

    rb_define_alloc_func(classVariadicInvoker, variadic_allocate);

    rb_define_method(classVariadicInvoker, "initialize", variadic_initialize, 4);
    rb_define_method(classVariadicInvoker, "invoke", variadic_call, 2);
    rb_define_alias(classVariadicInvoker, "call", "invoke");
}
