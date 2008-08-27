#include <sys/types.h>
#include <stdio.h>
#include <dlfcn.h>
#include <ruby.h>

#ifndef MACOSX
#  define MACOSX
#endif
#include <ffi/ffi.h>

#include "rbffi.h"

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
static VALUE invoker_attach(VALUE self, VALUE mod, VALUE methodName);
static void invoker_mark(Invoker *);
static void invoker_free(Invoker *);
static VALUE invoker_call(int argc, VALUE* argv, VALUE self);

void Init_ffi();

static VALUE moduleFFI = Qnil;
static VALUE classInvoker = Qnil;
static VALUE moduleNativeType = Qnil;

static ffi_type*
getFFIType(int paramType) {
    ffi_type* ffiType;

    switch (paramType) {
        case VOID:
            return &ffi_type_void;
        case INT8:
            return &ffi_type_sint8;
        case UINT8:
            return &ffi_type_uint8;
        case INT32:
            return &ffi_type_sint32;
        case UINT32:
            return &ffi_type_uint32;
        case INT64:
            return &ffi_type_sint64;
        case UINT64:
            return &ffi_type_uint64;
        case FLOAT32:
            return &ffi_type_float;
        case FLOAT64:
            return &ffi_type_double;
        case STRING:
        case RBXSTRING:
        case POINTER:
            return &ffi_type_pointer;
        default:
            return NULL;
    }
}
static VALUE
invoker_new(VALUE self, VALUE libname, VALUE cname, VALUE parameterTypes, 
        VALUE returnType, VALUE convention)
{
    Invoker* invoker = NULL;
    int paramCount;
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
        invoker->ffiParamTypes[i] = getFFIType(paramType);
        if (invoker->ffiParamTypes[i] == NULL) {
            errmsg = "Invalid parameter type";
            goto error;
        }
    }
    invoker->returnType = FIX2INT(returnType);
    ffiReturnType = getFFIType(invoker->returnType);
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
    int pointerCount = 0;
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
        }
        ffiValues[i] = &params[i];
    }
    ffi_call(&invoker->cif, FFI_FN(invoker->function), &retval, ffiValues);
    switch (invoker->returnType) {
        case VOID:
            return Qnil;        
        case INT8:
        case INT16:
        case INT32:
            return INT2FIX(retval.i);
        case UINT8:
        case UINT16:
        case UINT32:
            return INT2FIX(retval.u);
        case FLOAT32:
            return rb_float_new(retval.f32);
        case FLOAT64:
            return rb_float_new(retval.f64);
        case STRING:
            return rb_str_new2((char *) retval.ptr);
        default:
            rb_raise(rb_eRuntimeError, "Unknown return type: %d", invoker->returnType);
    }
    return Qnil;
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
Init_ffi() {
    moduleFFI = rb_define_module("FFI");
    classInvoker = rb_define_class_under(moduleFFI, "Invoker", rb_cObject);
    rb_define_singleton_method(classInvoker, "new", invoker_new, 5);
    rb_define_method(classInvoker, "call", invoker_call, -1);
    moduleNativeType = rb_define_module_under(moduleFFI, "NativeType");
    
    rb_define_const(moduleNativeType, "VOID", INT2FIX(VOID));
    rb_define_const(moduleNativeType, "INT8", INT2FIX(INT8));
    rb_define_const(moduleNativeType, "UINT8", INT2FIX(UINT8));
    rb_define_const(moduleNativeType, "INT16", INT2FIX(INT16));
    rb_define_const(moduleNativeType, "UINT16", INT2FIX(UINT16));
    rb_define_const(moduleNativeType, "INT32", INT2FIX(INT32));
    rb_define_const(moduleNativeType, "UINT32", INT2FIX(UINT32));
    rb_define_const(moduleNativeType, "INT64", INT2FIX(INT64));
    rb_define_const(moduleNativeType, "UINT64", INT2FIX(UINT64));
    rb_define_const(moduleNativeType, "FLOAT32", INT2FIX(FLOAT32));
    rb_define_const(moduleNativeType, "FLOAT64", INT2FIX(FLOAT64));
    rb_define_const(moduleNativeType, "POINTER", INT2FIX(POINTER));
    rb_define_const(moduleNativeType, "STRING", INT2FIX(STRING));
    rb_define_const(moduleNativeType, "RBXSTRING", INT2FIX(RBXSTRING));
    rb_define_const(moduleNativeType, "CHAR_ARRAY", INT2FIX(CHAR_ARRAY));
    if (sizeof(long) == 4) {
        rb_define_const(moduleNativeType, "LONG", INT2FIX(INT32));
        rb_define_const(moduleNativeType, "ULONG", INT2FIX(UINT32));
    } else {
        rb_define_const(moduleNativeType, "LONG", INT2FIX(INT64));
        rb_define_const(moduleNativeType, "ULONG", INT2FIX(UINT64));
    }
}