#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <ruby.h>

#include <ffi.h>

#include "rbffi.h"
#include "compat.h"

#include "AbstractMemory.h"
#include "Pointer.h"
#include "Platform.h"
#include "Callback.h"
#include "Types.h"

typedef struct Invoker {
    void* dlhandle;
    void* function;
    ffi_cif cif;
    int paramCount;
    ffi_type** ffiParamTypes;
    NativeType* paramTypes;
    NativeType returnType;
    VALUE callbackArray;
    int callbackCount;
    VALUE* callbackParameters;
} Invoker;

static VALUE invoker_new(VALUE self, VALUE libname, VALUE cname, VALUE parameterTypes, 
        VALUE returnType, VALUE convention);
static void invoker_mark(Invoker *);
static void invoker_free(Invoker *);
static VALUE invoker_call(int argc, VALUE* argv, VALUE self);
static VALUE invoker_call0(VALUE self);
static VALUE invoker_arity(VALUE self);
static void* callback_param(VALUE proc, VALUE cbinfo);
static VALUE classInvoker = Qnil;
static ID cbTableID, to_ptr;

static VALUE
invoker_new(VALUE self, VALUE libname, VALUE cname, VALUE parameterTypes, 
        VALUE returnType, VALUE convention)
{
    Invoker* invoker = NULL;
    ffi_type* ffiReturnType;
    ffi_abi abi;
    ffi_status ffiStatus;
    VALUE retval = Qnil;
    int i;

    Check_Type(cname, T_STRING);
    Check_Type(parameterTypes, T_ARRAY);
    Check_Type(returnType, T_FIXNUM);
    Check_Type(convention, T_STRING);

    retval = Data_Make_Struct(classInvoker, Invoker, invoker_mark, invoker_free, invoker);
    invoker->dlhandle = dlopen(libname != Qnil ? StringValuePtr(libname) : NULL, RTLD_LAZY);
    if (invoker->dlhandle == NULL) {
        rb_raise(rb_eLoadError, "No such library: %s", StringValuePtr(libname));
    }
    invoker->function = dlsym(invoker->dlhandle, StringValuePtr(cname));
    if (invoker->function == NULL) {
        rb_raise(rb_eLoadError, "Could not locate function '%s' in library '%s'",
                StringValuePtr(cname), libname != Qnil ? StringValuePtr(libname) : "[current process]");
    }
    invoker->paramCount = RARRAY_LEN(parameterTypes);
    invoker->paramTypes = ALLOC_N(NativeType, invoker->paramCount);
    invoker->ffiParamTypes = ALLOC_N(ffi_type *, invoker->paramCount);

    for (i = 0; i < invoker->paramCount; ++i) {
        VALUE entry = rb_ary_entry(parameterTypes, i);
        if (rb_obj_is_kind_of(entry, rb_FFI_Callback_class)) {
            invoker->callbackParameters = REALLOC_N(invoker->callbackParameters, VALUE,
                    invoker->callbackCount + 1);
            invoker->callbackParameters[invoker->callbackCount++] = entry;
            invoker->paramTypes[i] = CALLBACK;
            invoker->ffiParamTypes[i] = &ffi_type_pointer;            
        } else {
            int paramType = FIX2INT(entry);
            invoker->paramTypes[i] = paramType;
            invoker->ffiParamTypes[i] = rb_FFI_NativeTypeToFFI(paramType);
        }
        if (invoker->ffiParamTypes[i] == NULL) {
            rb_raise(rb_eArgError, "Invalid parameter type");
        }
    }
    invoker->returnType = FIX2INT(returnType);
    ffiReturnType = rb_FFI_NativeTypeToFFI(invoker->returnType);
    if (ffiReturnType == NULL) {
        rb_raise(rb_eArgError, "Invalid return type");
    }
#ifdef _WIN32
    abi = strcmp(StringValueCPtr(convention), "stdcall") == 0 ? FFI_STDCALL : FFI_DEFAULT_ABI;
#else
    abi = FFI_DEFAULT_ABI;
#endif
    ffiStatus = ffi_prep_cif(&invoker->cif, abi, invoker->paramCount,
            ffiReturnType, invoker->ffiParamTypes);
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
    
    return retval;
}
typedef union {
    signed long i;
    unsigned long u;
    signed long long i64;
    unsigned long long u64;
    void* ptr;
    float f32;
    double f64;
} FFIStorage;

static void
ffi_arg_setup(Invoker* invoker, int argc, VALUE* argv, FFIStorage *params, void** ffiValues)
{
    VALUE callbackProc = Qnil;
    int i, argidx, cbidx, type;
    if (argc < invoker->paramCount && invoker->callbackCount == 1 && rb_block_given_p()) {
        callbackProc = rb_block_proc();
    } else if (argc != invoker->paramCount) {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", argc, invoker->paramCount);
    }

    for (i = 0, argidx = 0, cbidx = 0; i < invoker->paramCount; ++i) {
        switch (invoker->paramTypes[i]) {
            case INT8:
            case INT16:
            case INT32:
                Check_Type(argv[argidx], T_FIXNUM);
                params[i].i = NUM2INT(argv[argidx]);
                ++argidx;
                break;
            case UINT8:
            case UINT16:
            case UINT32:
                Check_Type(argv[argidx], T_FIXNUM);
                params[i].u = NUM2UINT(argv[argidx]);
                ++argidx;
                break;
            case INT64:
                Check_Type(argv[argidx], T_FIXNUM);
                params[i].i64 = NUM2LL(argv[argidx]);
                ++argidx;
                break;
            case UINT64:
                Check_Type(argv[argidx], T_FIXNUM);
                params[i].i64 = NUM2ULL(argv[argidx]);
                ++argidx;
                break;
            case FLOAT32:
                if (TYPE(argv[argidx]) != T_FLOAT && TYPE(argv[argidx]) != T_FIXNUM) {
                    Check_Type(argv[argidx], T_FLOAT);
                }
                params[i].f32 = (float) NUM2DBL(argv[argidx]);
                ++argidx;
                break;
            case FLOAT64:
                if (TYPE(argv[argidx]) != T_FLOAT && TYPE(argv[argidx]) != T_FIXNUM) {
                    Check_Type(argv[argidx], T_FLOAT);
                }
                params[i].f64 = NUM2DBL(argv[argidx]);
                ++argidx;
                break;
            case STRING:
                type = TYPE(argv[argidx]);
                if (type == T_STRING) {
                    if (rb_safe_level() >= 1 && OBJ_TAINTED(argv[argidx])) {
                        rb_raise(rb_eSecurityError, "Unsafe string parameter");
                    }
                    params[i].ptr = StringValuePtr(argv[argidx]);
                } else if (type == T_NIL) {
                    params[i].ptr = NULL;
                } else {
                    rb_raise(rb_eArgError, "Invalid String value");
                }
                ++argidx;
                break;
            case POINTER:
            case BUFFER_IN:
            case BUFFER_OUT:
            case BUFFER_INOUT:
                type = TYPE(argv[argidx]);
                if (rb_obj_is_kind_of(argv[argidx], rb_FFI_AbstractMemory_class) && type == T_DATA) {
                    params[i].ptr = ((AbstractMemory *) DATA_PTR(argv[argidx]))->address;
                } else if (type == T_STRING) {
                    if (rb_safe_level() >= 1 && OBJ_TAINTED(argv[argidx])) {
                        rb_raise(rb_eSecurityError, "Unsafe string parameter");
                    }
                    params[i].ptr = StringValuePtr(argv[argidx]);
                } else if (type == T_NIL) {
                    params[i].ptr = NULL;
                } else if (type == T_FIXNUM) {
                    params[i].ptr = (void *) (uintptr_t) FIX2INT(argv[argidx]);
                } else if (type == T_BIGNUM) {
                    params[i].ptr = (void *) (uintptr_t) NUM2ULL(argv[argidx]);
                } else if (rb_respond_to(argv[argidx], to_ptr)) {
                    VALUE ptr = rb_funcall2(argv[argidx], to_ptr, 0, NULL);
                    if (rb_obj_is_kind_of(ptr, rb_FFI_Pointer_class) && TYPE(ptr) == T_DATA) {
                        params[i].ptr = ((AbstractMemory *) DATA_PTR(ptr))->address;
                    } else {
                        rb_raise(rb_eArgError, "to_ptr returned an invalid pointer");
                    }

                } else {
                    rb_raise(rb_eArgError, ":pointer argument is not a valid pointer");
                }
                ++argidx;
                break;
            case CALLBACK:
                if (callbackProc != Qnil) {
                    params[i].ptr = callback_param(callbackProc, invoker->callbackParameters[cbidx++]);
                } else {
                    params[i].ptr = callback_param(argv[argidx], invoker->callbackParameters[cbidx++]);
                    ++argidx;
                }
                break;
            default:
                rb_raise(rb_eArgError, "Invalid parameter type: %d", invoker->paramTypes[i]);
        }
        ffiValues[i] = &params[i];
    }
}
static VALUE
invoker_call(int argc, VALUE* argv, VALUE self)
{
    Invoker* invoker;
    FFIStorage params[MAX_PARAMETERS], retval;
    void* ffiValues[MAX_PARAMETERS];

    Data_Get_Struct(self, Invoker, invoker);
    ffi_arg_setup(invoker, argc, argv, params, ffiValues);
    ffi_call(&invoker->cif, FFI_FN(invoker->function), &retval, ffiValues);
    return rb_FFI_NativeValueToRuby(invoker->returnType, &retval);
}

static VALUE
invoker_call0(VALUE self)
{
    Invoker* invoker;
    void* ffiValues[] = { NULL };
    FFIStorage retval;
    
    Data_Get_Struct(self, Invoker, invoker);
    ffi_call(&invoker->cif, FFI_FN(invoker->function), &retval, ffiValues);
    return rb_FFI_NativeValueToRuby(invoker->returnType, &retval);
}

static VALUE
invoker_call1(VALUE self, VALUE arg1)
{
    Invoker* invoker;
    void* ffiValues[1];
    FFIStorage retval, params[1];

    Data_Get_Struct(self, Invoker, invoker);
    ffi_arg_setup(invoker, 1, &arg1, params, ffiValues);
    ffi_call(&invoker->cif, FFI_FN(invoker->function), &retval, ffiValues);
    return rb_FFI_NativeValueToRuby(invoker->returnType, &retval);
}

static VALUE
invoker_call2(VALUE self, VALUE arg1, VALUE arg2)
{
    Invoker* invoker;
    void* ffiValues[2];
    FFIStorage retval, params[2];
    VALUE argv[] = { arg1, arg2 };

    Data_Get_Struct(self, Invoker, invoker);
    ffi_arg_setup(invoker, 2, argv, params, ffiValues);
    ffi_call(&invoker->cif, FFI_FN(invoker->function), &retval, ffiValues);
    return rb_FFI_NativeValueToRuby(invoker->returnType, &retval);
}

static VALUE
invoker_call3(VALUE self, VALUE arg1, VALUE arg2, VALUE arg3)
{
    Invoker* invoker;
    void* ffiValues[3];
    FFIStorage retval, params[3];
    VALUE argv[] = { arg1, arg2, arg3 };

    Data_Get_Struct(self, Invoker, invoker);
    ffi_arg_setup(invoker, 3, argv, params, ffiValues);
    ffi_call(&invoker->cif, FFI_FN(invoker->function), &retval, ffiValues);
    return rb_FFI_NativeValueToRuby(invoker->returnType, &retval);
}

static VALUE
invoker_arity(VALUE self)
{
    Invoker* invoker;
    Data_Get_Struct(self, Invoker, invoker);
    return INT2FIX(invoker->paramCount);
}

static void
invoker_mark(Invoker *invoker)
{
    if (invoker->callbackCount > 0) {
        rb_gc_mark_locations(&invoker->callbackParameters[0], &invoker->callbackParameters[invoker->callbackCount]);
    }
}

static void
invoker_free(Invoker *invoker)
{
    if (invoker != NULL) {
        if (invoker->dlhandle != NULL) {
            dlclose(invoker->dlhandle);
            invoker->dlhandle = NULL;
        }
        if (invoker->paramTypes != NULL) {
            xfree(invoker->paramTypes);
            invoker->paramTypes = NULL;
        }
        if (invoker->ffiParamTypes != NULL) {
            xfree(invoker->ffiParamTypes);
            invoker->ffiParamTypes = NULL;
        }
        if (invoker->callbackParameters != NULL) {
            xfree(invoker->callbackParameters);
            invoker->callbackParameters = NULL;
        }
        xfree(invoker);
    }
}

static void* 
callback_param(VALUE proc, VALUE cbInfo)
{
    VALUE callback;
    VALUE cbTable = rb_ivar_get(proc, cbTableID);
    if (!cbTable || cbTable == Qnil) {
        cbTable = rb_hash_new();
        rb_ivar_set(proc, cbTableID, cbTable);
    }
    callback = rb_hash_aref(cbTable, cbInfo);
    if (callback != Qnil) {
        return ((NativeCallback *) DATA_PTR(callback))->code;
    }
    callback = rb_FFI_NativeCallback_new(cbInfo, proc);
    rb_hash_aset(cbTable, cbInfo, callback);
    return ((NativeCallback *) DATA_PTR(callback))->code;
}
void 
rb_FFI_Invoker_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    classInvoker = rb_define_class_under(moduleFFI, "Invoker", rb_cObject);
    rb_define_singleton_method(classInvoker, "new", invoker_new, 5);
    rb_define_method(classInvoker, "call", invoker_call, -1);
    rb_define_method(classInvoker, "call0", invoker_call0, 0);
    rb_define_method(classInvoker, "call1", invoker_call1, 1);
    rb_define_method(classInvoker, "call2", invoker_call2, 2);
    rb_define_method(classInvoker, "call3", invoker_call3, 3);
    rb_define_method(classInvoker, "arity", invoker_arity, 0);
    cbTableID = rb_intern("__ffi_callback_table__");
    to_ptr = rb_intern("to_ptr");
}
