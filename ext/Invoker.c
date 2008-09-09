#include <sys/types.h>
#include <stdio.h>
#include <dlfcn.h>
#include <ruby.h>

#include <ffi.h>

#include "rbffi.h"
#include "AbstractMemory.h"
#include "MemoryPointer.h"
#include "Platform.h"
#include "Types.h"

typedef struct Invoker {
    void* dlhandle;
    void* function;
    ffi_cif cif;
    int paramCount;
    ffi_type** ffiParamTypes;
    NativeType* paramTypes;
    NativeType returnType;
} Invoker;

static VALUE invoker_new(VALUE self, VALUE libname, VALUE cname, VALUE parameterTypes, 
        VALUE returnType, VALUE convention);
static void invoker_mark(Invoker *);
static void invoker_free(Invoker *);
static VALUE invoker_call(int argc, VALUE* argv, VALUE self);

static VALUE classInvoker = Qnil;

static VALUE
invoker_new(VALUE self, VALUE libname, VALUE cname, VALUE parameterTypes, 
        VALUE returnType, VALUE convention)
{
    Invoker* invoker = NULL;
    ffi_type* ffiReturnType;
    ffi_abi abi;
    ffi_status ffiStatus;
    const char* errmsg = "Failed to create invoker";
    int i;

    Check_Type(cname, T_STRING);
    Check_Type(libname, T_STRING);
    Check_Type(parameterTypes, T_ARRAY);
    Check_Type(returnType, T_FIXNUM);
    Check_Type(convention, T_STRING);
    
    invoker = ALLOC(Invoker);
    MEMZERO(invoker, Invoker, 1);
    
    invoker->paramCount = RARRAY(parameterTypes)->len;
    invoker->paramTypes = ALLOC_N(NativeType, invoker->paramCount);
    invoker->ffiParamTypes = ALLOC_N(ffi_type *, invoker->paramCount);
    for (i = 0; i < invoker->paramCount; ++i) {
        int paramType = FIX2INT(rb_ary_entry(parameterTypes, i));
        invoker->paramTypes[i] = paramType;
        invoker->ffiParamTypes[i] = rb_ffi_NativeTypeToFFI(paramType);
        if (invoker->ffiParamTypes[i] == NULL) {
            errmsg = "Invalid parameter type";
            goto error;
        }
    }
    invoker->returnType = FIX2INT(returnType);
    ffiReturnType = rb_ffi_NativeTypeToFFI(invoker->returnType);
    if (ffiReturnType == NULL) {
        errmsg = "Invalid return type";
        goto error;
    }
#ifdef _WIN32
    abi = strcmp(StringValuePtr(convention), "stdcall") == 0 ? FFI_STDCALL : FFI_DEFAULT_ABI;
#else
    abi = FFI_DEFAULT_ABI;
#endif
    ffiStatus = ffi_prep_cif(&invoker->cif, abi, invoker->paramCount,
            ffiReturnType, invoker->ffiParamTypes);
    if (ffiStatus != FFI_OK) {
        errmsg = "ffi_prep_cif failed";
        goto error;
    }

    invoker->dlhandle = dlopen(StringValuePtr(libname), RTLD_LAZY);
    if (invoker->dlhandle == NULL) {
        errmsg = "No such library";
        goto error;
    }
    invoker->function = dlsym(invoker->dlhandle, StringValuePtr(cname));
    if (invoker->function == NULL) {
        errmsg = "Could not locate function within library";
        goto error;
    }
    return Data_Wrap_Struct(classInvoker, invoker_mark, invoker_free, invoker);
error:
    if (invoker != NULL) {
        if (invoker->dlhandle != NULL) {
            dlclose(invoker->dlhandle);
        }
        if (invoker->paramTypes != NULL) {
            xfree(invoker->paramTypes);
        }
        if (invoker->ffiParamTypes != NULL) {
            xfree(invoker->ffiParamTypes);
        }
        xfree(invoker);        
    }
    rb_raise(rb_eRuntimeError, errmsg);
}

static VALUE
invoker_call(int argc, VALUE* argv, VALUE self)
{
    Invoker* invoker;
    union {
        signed long i;
        unsigned long u;
        signed long long i64;
        unsigned long long u64;
        void* ptr;
        float f32;
        double f64;
    } params[MAX_PARAMETERS], retval;
    void* ffiValues[MAX_PARAMETERS];    
    int i;

    Data_Get_Struct(self, Invoker, invoker);
    if (argc != invoker->paramCount) {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", argc, invoker->paramCount);
    }

    for (i = 0; i < invoker->paramCount; ++i) {
        switch (invoker->paramTypes[i]) {
            case INT8:
            case INT16:
            case INT32:
                Check_Type(argv[i], T_FIXNUM);
                params[i].i = NUM2INT(argv[i]);
                break;
            case UINT8:
            case UINT16:
            case UINT32:
                Check_Type(argv[i], T_FIXNUM);
                params[i].u = NUM2UINT(argv[i]);
                break;
            case INT64:
                Check_Type(argv[i], T_FIXNUM);
                params[i].i64 = NUM2LL(argv[i]);
                break;
            case UINT64:
                Check_Type(argv[i], T_FIXNUM);
                params[i].i64 = NUM2ULL(argv[i]);
                break;
            case FLOAT32:
                if (TYPE(argv[i]) != T_FLOAT && TYPE(argv[i]) != T_FIXNUM) {
                    Check_Type(argv[i], T_FLOAT);
                }
                params[i].f32 = (float) NUM2DBL(argv[i]);
                break;
            case FLOAT64:
                if (TYPE(argv[i]) != T_FLOAT && TYPE(argv[i]) != T_FIXNUM) {
                    Check_Type(argv[i], T_FLOAT);
                }
                params[i].f64 = NUM2DBL(argv[i]);
                break;
            case STRING:
                Check_Type(argv[i], T_STRING);
                params[i].ptr = StringValuePtr(argv[i]);
                break;
            case POINTER:
                if (rb_obj_is_kind_of(argv[i], rb_FFI_AbstractMemory_class)) {
                    params[i].ptr = ((AbstractMemory *) DATA_PTR(argv[i]))->address;
                } else if (TYPE(argv[i]) == T_STRING) {
                    params[i].ptr = StringValuePtr(argv[i]);
                } else if (TYPE(argv[i] == T_NIL)) {
                    params[i].ptr = NULL;
                } else if (TYPE(argv[i] == T_FIXNUM)) {
                    params[i].ptr = (void *) (uintptr_t) FIX2INT(argv[i]);
                } else if (TYPE(argv[i] == T_BIGNUM)) {
                    params[i].ptr = (void *) (uintptr_t) NUM2ULL(argv[i]);
                } else {
                    rb_raise(rb_eArgError, ":pointer argument is not a valid pointer");
                }
                break;
            default:
                rb_raise(rb_eArgError, "Invalid parameter type: %d", invoker->paramTypes[i]);
        }
        ffiValues[i] = &params[i];
    }
    ffi_call(&invoker->cif, FFI_FN(invoker->function), &retval, ffiValues);
    return rb_ffi_NativeValueToRuby(invoker->returnType, &retval);
}

static void
invoker_mark(Invoker *invoker)
{

}
static void
invoker_free(Invoker *invoker)
{
    xfree(invoker->paramTypes);
    xfree(invoker->ffiParamTypes);
    dlclose(invoker->dlhandle);
    xfree(invoker);
}


void 
rb_FFI_Invoker_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    classInvoker = rb_define_class_under(moduleFFI, "Invoker", rb_cObject);
    rb_define_singleton_method(classInvoker, "new", invoker_new, 5);
    rb_define_method(classInvoker, "call", invoker_call, -1);
    
}
