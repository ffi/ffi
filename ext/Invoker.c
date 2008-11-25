#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <errno.h>
#include <ruby.h>

#include <ffi.h>
#ifdef HAVE_NATIVETHREAD
# include <pthread.h>
#endif
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

#ifdef HAVE_NATIVETHREAD
static pthread_key_t threadDataKey;
#endif

#define threadData (thread_data_get())

#if defined(__i386__)
#  define USE_RAW
#endif

#ifdef USE_RAW
#  ifndef __i386__
#    error "RAW argument packing only supported on i386"
#  endif

#define INT8_SIZE (sizeof(char))
#define INT16_SIZE (sizeof(short))
#define INT32_SIZE (sizeof(int))
#define INT64_SIZE (sizeof(long long))
#define FLOAT32_SIZE (sizeof(float))
#define FLOAT64_SIZE (sizeof(double))
#define ADDRESS_SIZE (sizeof(void *))
#define INT8_ADJ (4)
#define INT16_ADJ (4)
#define INT32_ADJ (4)
#define INT64_ADJ (8)
#define FLOAT32_ADJ (4)
#define FLOAT64_ADJ (8)
#define ADDRESS_ADJ (sizeof(void *))

#endif /* USE_RAW */

#ifdef USE_RAW
#  define ADJ(p, a) ((p) = (FFIStorage*) (((caddr_t) p) + a##_ADJ))
#else
#  define ADJ(p, a) (++(p))
#endif


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
        if (rb_obj_is_kind_of(entry, rb_FFI_CallbackInfo_class)) {
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

static inline int
getSignedInt(VALUE value, int type, int minValue, int maxValue, const char* typeName)
{
    int i;
    if (type != T_FIXNUM && type != T_BIGNUM) {
        rb_raise(rb_eTypeError, "Expected an Integer parameter");
    }
    i = NUM2INT(value);
    if (i < minValue || i > maxValue) {
        rb_raise(rb_eRangeError, "Value %d outside %s range", i, typeName);
    }
    return i;
}

static inline int
getUnsignedInt(VALUE value, int type, int maxValue, const char* typeName)
{
    int i;
    if (type != T_FIXNUM && type != T_BIGNUM) {
        rb_raise(rb_eTypeError, "Expected an Integer parameter");
    }
    i = NUM2INT(value);
    if (i < 0 || i > maxValue) {
        rb_raise(rb_eRangeError, "Value %d outside %s range", i, typeName);
    }
    return i;
}

/* Special handling/checking for unsigned 32 bit integers */
static inline unsigned int
getUnsignedInt32(VALUE value, int type)
{
    long long i;
    if (type != T_FIXNUM && type != T_BIGNUM) {
        rb_raise(rb_eTypeError, "Expected an Integer parameter");
    }
    i = NUM2LL(value);
    if (i < 0L || i > 0xffffffffL) {
        rb_raise(rb_eRangeError, "Value %lld outside unsigned int range", i);
    }
    return (unsigned int) i;
}

static void
ffi_arg_setup(const Invoker* invoker, int argc, VALUE* argv, NativeType* paramTypes,
        FFIStorage* paramStorage, void** ffiValues)
{
    VALUE callbackProc = Qnil;
    FFIStorage* param = &paramStorage[0];
    int i, argidx, cbidx, argCount;

    if (invoker->paramCount != -1 && invoker->paramCount != argc) {
        if (argc == (invoker->paramCount - 1) && invoker->callbackCount == 1 && rb_block_given_p()) {
            callbackProc = rb_block_proc();
        } else {
            rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", argc, invoker->paramCount);
        }
    }
    argCount = invoker->paramCount != -1 ? invoker->paramCount : argc;
    for (i = 0, argidx = 0, cbidx = 0; i < argCount; ++i) {
        int type = argidx < argc ? TYPE(argv[argidx]) : T_NONE;
        ffiValues[i] = param;
        
        switch (paramTypes[i]) {
            case INT8:
                param->i = getSignedInt(argv[argidx++], type, -128, 127, "char");
                ADJ(param, INT8);
                break;
            case INT16:
                param->i = getSignedInt(argv[argidx++], type, -0x8000, 0x7fff, "short");
                ADJ(param, INT16);
                break;
            case INT32:
                param->i = getSignedInt(argv[argidx++], type, -0x80000000, 0x7fffffff, "int");
                ADJ(param, INT32);
                break;
            case UINT8:
                param->u = getUnsignedInt(argv[argidx++], type, 0xff, "unsigned char");
                ADJ(param, INT8);
                break;
            case UINT16:
                param->u = getUnsignedInt(argv[argidx++], type, 0xffff, "unsigned short");
                ADJ(param, INT16);
                break;
            case UINT32:
                /* Special handling/checking for unsigned 32 bit integers */
                param->u = getUnsignedInt32(argv[argidx++], type);
                ADJ(param, INT32);
                break;
            case INT64:
                if (type != T_FIXNUM && type != T_BIGNUM) {
                    rb_raise(rb_eTypeError, "Expected an Integer parameter");
                }
                param->i64 = NUM2LL(argv[argidx]);
                ADJ(param, INT64);
                ++argidx;
                break;
            case UINT64:
                if (type != T_FIXNUM && type != T_BIGNUM) {
                    rb_raise(rb_eTypeError, "Expected an Integer parameter");
                }
                param->i64 = NUM2ULL(argv[argidx]);
                ADJ(param, INT64);
                ++argidx;
                break;
            case FLOAT32:
                if (type != T_FLOAT && type != T_FIXNUM) {
                    rb_raise(rb_eTypeError, "Expected a Float parameter");
                }
                param->f32 = (float) NUM2DBL(argv[argidx]);
                ADJ(param, FLOAT32);
                ++argidx;
                break;
            case FLOAT64:
                if (type != T_FLOAT && type != T_FIXNUM) {
                    rb_raise(rb_eTypeError, "Expected a Float parameter");
                }
                param->f64 = NUM2DBL(argv[argidx]);
                ADJ(param, FLOAT64);
                ++argidx;
                break;
            case STRING:
                if (type == T_STRING) {
                    if (rb_safe_level() >= 1 && OBJ_TAINTED(argv[argidx])) {
                        rb_raise(rb_eSecurityError, "Unsafe string parameter");
                    }
                    param->ptr = StringValuePtr(argv[argidx]);
                } else if (type == T_NIL) {
                    param->ptr = NULL;
                } else {
                    rb_raise(rb_eArgError, "Invalid String value");
                }
                ADJ(param, ADDRESS);
                ++argidx;
                break;
            case POINTER:
            case BUFFER_IN:
            case BUFFER_OUT:
            case BUFFER_INOUT:
                if (rb_obj_is_kind_of(argv[argidx], rb_FFI_AbstractMemory_class) && type == T_DATA) {
                    param->ptr = ((AbstractMemory *) DATA_PTR(argv[argidx]))->address;
                } else if (type == T_STRING) {
                    if (rb_safe_level() >= 1 && OBJ_TAINTED(argv[argidx])) {
                        rb_raise(rb_eSecurityError, "Unsafe string parameter");
                    }
                    param->ptr = StringValuePtr(argv[argidx]);
                } else if (type == T_NIL) {
                    param->ptr = NULL;
                } else if (type == T_FIXNUM) {
                    param->ptr = (void *) (uintptr_t) FIX2INT(argv[argidx]);
                } else if (type == T_BIGNUM) {
                    param->ptr = (void *) (uintptr_t) NUM2ULL(argv[argidx]);
                } else if (rb_respond_to(argv[argidx], to_ptr)) {
                    VALUE ptr = rb_funcall2(argv[argidx], to_ptr, 0, NULL);
                    if (rb_obj_is_kind_of(ptr, rb_FFI_Pointer_class) && TYPE(ptr) == T_DATA) {
                        param->ptr = ((AbstractMemory *) DATA_PTR(ptr))->address;
                    } else {
                        rb_raise(rb_eArgError, "to_ptr returned an invalid pointer");
                    }

                } else {
                    rb_raise(rb_eArgError, ":pointer argument is not a valid pointer");
                }
                ADJ(param, ADDRESS);
                ++argidx;
                break;
            case CALLBACK:
                if (callbackProc != Qnil) {
                    param->ptr = callback_param(callbackProc, invoker->callbackParameters[cbidx++]);
                } else {
                    param->ptr = callback_param(argv[argidx], invoker->callbackParameters[cbidx++]);
                    ++argidx;
                }
                ADJ(param, ADDRESS);
                break;
            default:
                rb_raise(rb_eArgError, "Invalid parameter type: %d", paramTypes[i]);
        }
    }
}
static inline VALUE
ffi_invoke(ffi_cif* cif, void* function, NativeType returnType, void** ffiValues)
{
    FFIStorage retval;
#ifdef USE_RAW
    ffi_raw_call(cif, FFI_FN(function), &retval, (ffi_raw *) ffiValues[0]);
#else
    ffi_call(cif, FFI_FN(function), &retval, ffiValues);
#endif
    threadData->td_errno = errno;
    return rb_FFI_NativeValueToRuby(returnType, &retval);
}
static VALUE
invoker_call(int argc, VALUE* argv, VALUE self)
{
    Invoker* invoker;
    FFIStorage params[MAX_PARAMETERS];
    void* ffiValues[MAX_PARAMETERS];

    Data_Get_Struct(self, Invoker, invoker);
    ffi_arg_setup(invoker, argc, argv, invoker->paramTypes, params, ffiValues);
    return ffi_invoke(&invoker->cif, invoker->function, invoker->returnType, ffiValues);
}

static VALUE
invoker_call0(VALUE self)
{
    Invoker* invoker;
    FFIStorage arg0;
    void* ffiValues[] = { &arg0 };
    
    Data_Get_Struct(self, Invoker, invoker);
    return ffi_invoke(&invoker->cif, invoker->function, invoker->returnType, ffiValues);
}

static VALUE
invoker_call1(VALUE self, VALUE arg1)
{
    Invoker* invoker;
    void* ffiValues[1];
    FFIStorage params[1];

    Data_Get_Struct(self, Invoker, invoker);
    ffi_arg_setup(invoker, 1, &arg1, invoker->paramTypes, params, ffiValues);
    return ffi_invoke(&invoker->cif, invoker->function, invoker->returnType, ffiValues);
}

static VALUE
invoker_call2(VALUE self, VALUE arg1, VALUE arg2)
{
    Invoker* invoker;
    void* ffiValues[2];
    FFIStorage params[2];
    VALUE argv[] = { arg1, arg2 };

    Data_Get_Struct(self, Invoker, invoker);
    ffi_arg_setup(invoker, 2, argv, invoker->paramTypes, params, ffiValues);
    return ffi_invoke(&invoker->cif, invoker->function, invoker->returnType, ffiValues);
}

static VALUE
invoker_call3(VALUE self, VALUE arg1, VALUE arg2, VALUE arg3)
{
    Invoker* invoker;
    void* ffiValues[3];
    FFIStorage params[3];
    VALUE argv[] = { arg1, arg2, arg3 };

    Data_Get_Struct(self, Invoker, invoker);
    ffi_arg_setup(invoker, 3, argv, invoker->paramTypes, params, ffiValues);
    return ffi_invoke(&invoker->cif, invoker->function, invoker->returnType, ffiValues);
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
        switch (paramType) {
            case INT8:
            case INT16:
            case INT32:
                paramType = INT32;
                break;
            case UINT8:
            case UINT16:
            case UINT32:
                paramType = UINT32;
                break;
            case FLOAT32:
                paramType = FLOAT64;
                break;
        }
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
    return ffi_invoke(&cif, invoker->function, invoker->returnType, ffiValues);
}

static void* 
callback_param(VALUE proc, VALUE cbInfo)
{
    VALUE callback;
    VALUE cbTable = RTEST(rb_ivar_defined(proc, cbTableID)) ? rb_ivar_get(proc, cbTableID) : Qnil;
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
#ifdef HAVE_NATIVETHREAD
static ThreadData*
thread_data_init()
{
    ThreadData* td = ALLOC_N(ThreadData, 1);
    memset(td, 0, sizeof(*td));
    pthread_setspecific(threadDataKey, td);
    return td;
}
static inline ThreadData*
thread_data_get()
{
    ThreadData* td = pthread_getspecific(threadDataKey);
    return td != NULL ? td : thread_data_init();
}

static void
thread_data_free(void *ptr)
{
    xfree(ptr);
}
#else /* !HAVE_NATIVETHREAD */
static ThreadData td0;
static inline ThreadData*
thread_data_get()
{
    return &td0;
}
#endif
static VALUE
get_last_error(VALUE self)
{
    return INT2NUM(threadData->td_errno);
}
static VALUE
set_last_error(VALUE self, VALUE error)
{
    return Qnil;
}

void 
rb_FFI_Invoker_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    VALUE moduleError = rb_define_module_under(moduleFFI, "LastError");
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
    
    rb_define_module_function(moduleError, "error", get_last_error, 0);
    rb_define_module_function(moduleError, "error=", set_last_error, 1);
#ifdef HAVE_NATIVETHREAD
    pthread_key_create(&threadDataKey, thread_data_free);
#endif
}
