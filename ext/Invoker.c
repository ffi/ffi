#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <errno.h>
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
    VALUE library;
    void* function;
    ffi_cif cif;
    int paramCount;
    ffi_type** ffiParamTypes;
    NativeType* paramTypes;
    NativeType returnType;
    VALUE callbackArray;
    int callbackCount;
    VALUE* callbackParameters;
    ffi_abi abi;
} Invoker;
typedef struct ThreadData {
    int td_errno;
} ThreadData;
static VALUE invoker_new(VALUE self, VALUE library, VALUE function, VALUE parameterTypes,
        VALUE returnType, VALUE convention);
static void invoker_mark(Invoker *);
static void invoker_free(Invoker *);
static VALUE invoker_call(int argc, VALUE* argv, VALUE self);
static VALUE invoker_call0(VALUE self);
static VALUE invoker_arity(VALUE self);
static void* callback_param(VALUE proc, VALUE cbinfo);
static inline ThreadData* thread_data_get(void);
static VALUE classInvoker = Qnil, classVariadicInvoker = Qnil;
static ID cbTableID, to_ptr;
static pthread_key_t threadDataKey;

#define threadData (thread_data_get())

static VALUE
invoker_new(VALUE klass, VALUE library, VALUE function, VALUE parameterTypes,
        VALUE returnType, VALUE convention)
{
    Invoker* invoker = NULL;
    ffi_type* ffiReturnType;
    ffi_abi abi;
    ffi_status ffiStatus;
    VALUE retval = Qnil;
    int i;

    Check_Type(parameterTypes, T_ARRAY);
    Check_Type(returnType, T_FIXNUM);
    Check_Type(convention, T_STRING);
    Check_Type(library, T_DATA);
    Check_Type(function, T_DATA);
    
    retval = Data_Make_Struct(klass, Invoker, invoker_mark, invoker_free, invoker);
    invoker->library = library;
    invoker->function = ((AbstractMemory *) DATA_PTR(function))->address;
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

static VALUE
variadic_invoker_new(VALUE klass, VALUE library, VALUE function, VALUE returnType, VALUE convention)
{
    Invoker* invoker = NULL;
    VALUE retval = Qnil;

    Check_Type(returnType, T_FIXNUM);
    Check_Type(convention, T_STRING);
    Check_Type(library, T_DATA);
    Check_Type(function, T_DATA);

    retval = Data_Make_Struct(klass, Invoker, invoker_mark, invoker_free, invoker);
    invoker->library = library;
    invoker->function = ((AbstractMemory *) DATA_PTR(function))->address;
#ifdef _WIN32
    invoker->abi = strcmp(StringValueCPtr(convention), "stdcall") == 0 ? FFI_STDCALL : FFI_DEFAULT_ABI;
#else
    invoker->abi = FFI_DEFAULT_ABI;
#endif
    invoker->returnType = FIX2INT(returnType);
    invoker->paramCount = -1;
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
ffi_arg_setup(const Invoker* invoker, int argc, VALUE* argv, NativeType* paramTypes,
        FFIStorage *params, void** ffiValues)
{
    VALUE callbackProc = Qnil;
    int i, argidx, cbidx, argCount, type;

    if (invoker->paramCount != -1) {
        if (argc < invoker->paramCount && invoker->callbackCount == 1 && rb_block_given_p()) {
            callbackProc = rb_block_proc();
        } else if (argc != invoker->paramCount) {
            rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", argc, invoker->paramCount);
        }
    }
    argCount = invoker->paramCount != -1 ? invoker->paramCount : argc;
    for (i = 0, argidx = 0, cbidx = 0; i < argCount; ++i) {
        switch (paramTypes[i]) {
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
                rb_raise(rb_eArgError, "Invalid parameter type: %d", paramTypes[i]);
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
    ffi_arg_setup(invoker, argc, argv, invoker->paramTypes, params, ffiValues);
    ffi_call(&invoker->cif, FFI_FN(invoker->function), &retval, ffiValues);
    threadData->td_errno = errno;
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
    threadData->td_errno = errno;
    return rb_FFI_NativeValueToRuby(invoker->returnType, &retval);
}

static VALUE
invoker_call1(VALUE self, VALUE arg1)
{
    Invoker* invoker;
    void* ffiValues[1];
    FFIStorage retval, params[1];

    Data_Get_Struct(self, Invoker, invoker);
    ffi_arg_setup(invoker, 1, &arg1, invoker->paramTypes, params, ffiValues);
    ffi_call(&invoker->cif, FFI_FN(invoker->function), &retval, ffiValues);
    threadData->td_errno = errno;
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
    ffi_arg_setup(invoker, 2, argv, invoker->paramTypes, params, ffiValues);
    ffi_call(&invoker->cif, FFI_FN(invoker->function), &retval, ffiValues);
    threadData->td_errno = errno;
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
    ffi_arg_setup(invoker, 3, argv, invoker->paramTypes, params, ffiValues);
    ffi_call(&invoker->cif, FFI_FN(invoker->function), &retval, ffiValues);
    threadData->td_errno = errno;
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
    if (invoker->library != Qnil) {
        rb_gc_mark(invoker->library);
    }
}

static void
invoker_free(Invoker *invoker)
{
    if (invoker != NULL) {
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

static VALUE
variadic_invoker_call(VALUE self, VALUE parameterTypes, VALUE parameterValues)
{
    Invoker* invoker;
    FFIStorage* params, retval;
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

    Data_Get_Struct(self, Invoker, invoker);
    paramCount = RARRAY_LEN(parameterTypes);
    paramTypes = ALLOCA_N(NativeType, paramCount);
    ffiParamTypes = ALLOCA_N(ffi_type *, paramCount);
    params = ALLOCA_N(FFIStorage, paramCount);
    ffiValues = ALLOCA_N(void*, paramCount);
    argv = ALLOCA_N(VALUE, paramCount);

    for (i = 0; i < paramCount; ++i) {
        VALUE entry = rb_ary_entry(parameterTypes, i);
        int paramType = FIX2INT(entry);
        paramTypes[i] = paramType;
        ffiParamTypes[i] = rb_FFI_NativeTypeToFFI(paramType);
        if (ffiParamTypes[i] == NULL) {
            rb_raise(rb_eArgError, "Invalid parameter type #%x", paramType);
        }
        argv[i] = rb_ary_entry(parameterValues, i);
    }

    ffiReturnType = rb_FFI_NativeTypeToFFI(invoker->returnType);
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
    ffi_arg_setup(invoker, paramCount, argv, paramTypes, params, ffiValues);
    ffi_call(&cif, FFI_FN(invoker->function), &retval, ffiValues);
    threadData->td_errno = errno;
    return rb_FFI_NativeValueToRuby(invoker->returnType, &retval);
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
#if RUBY_VERSION_CODE >= 190
static inline ThreadData*
thread_data_get()
{
    ThreadData* td = pthread_getspecific(threadDataKey);
    if (td == null) {
        td = ALLOC_N(ThreadData, 1);
        memset(td, 0, sizeof(*td));
        pthread_setspecific(threadDataKey, td);
    }
    return  td;
}

static void
thread_data_free(void *ptr)
{
    xfree(ptr);
}
#else /* ruby version < 1.9.0 */
static inline ThreadData*
thread_data_get()
{
    static ThreadData td0;
    return &td0;
}
#endif
static VALUE
last_error(VALUE self)
{
    return INT2FIX(threadData->td_errno);
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
    classVariadicInvoker = rb_define_class_under(moduleFFI, "VariadicInvoker", rb_cObject);
    rb_define_singleton_method(classVariadicInvoker, "__new", variadic_invoker_new, 4);
    rb_define_method(classVariadicInvoker, "invoke", variadic_invoker_call, 2);
    cbTableID = rb_intern("__ffi_callback_table__");
    to_ptr = rb_intern("to_ptr");
    rb_define_module_function(moduleFFI, "last_error", last_error, 0);
#if RUBY_VERSION_CODE >= 190
    pthread_key_create(&threadDatakey, thread_data_free);
#endif
}
