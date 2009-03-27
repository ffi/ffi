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

#include <ffi.h>
#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32) && !defined(__WIN32__)
# include <pthread.h>
#endif
#include "rbffi.h"
#include "compat.h"

#include "AbstractMemory.h"
#include "Pointer.h"
#include "Struct.h"
#include "Platform.h"
#include "Callback.h"
#include "Types.h"

#if defined(__i386__) && !defined(_WIN32) && !defined(__WIN32__)
#  define USE_RAW
#endif
#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32) && !defined(__WIN32__)
#  define USE_PTHREAD_LOCAL
#endif
#define MAX_FIXED_ARITY (3)

typedef struct MethodHandle MethodHandle;
typedef struct Invoker Invoker;

struct Invoker {
    VALUE library;
    VALUE enums;
    void* function;
    ffi_cif cif;
    int paramCount;
    ffi_type** ffiParamTypes;
    NativeType* paramTypes;
    NativeType returnType;
    VALUE rbReturnType;
    VALUE callbackArray;
    int callbackCount;
    VALUE* callbackParameters;
    ffi_abi abi;
    MethodHandle* methodHandle;
};
#ifdef USE_RAW
#  define METHOD_CLOSURE ffi_raw_closure
#else
#  define METHOD_CLOSURE ffi_closure
#endif
typedef struct MethodHandlePool MethodHandlePool;
struct MethodHandle {
    Invoker* invoker;
    int arity;
    METHOD_CLOSURE* closure;
    void* code;
    ffi_cif cif;
    struct MethodHandlePool* pool;
    MethodHandle* next;
};
#if !defined(_WIN32) && !defined(__WIN32__)
struct MethodHandlePool {
#ifdef HAVE_NATIVETHREAD
    pthread_mutex_t mutex;
#endif
    MethodHandle* list;
};
#endif /* _WIN32 */
typedef struct ThreadData {
    int td_errno;
} ThreadData;
static VALUE invoker_allocate(VALUE klass);
static VALUE invoker_initialize(VALUE self, VALUE library, VALUE function, VALUE parameterTypes,
        VALUE rbReturnType, VALUE returnType, VALUE convention, VALUE enums);
static void invoker_mark(Invoker *);
static void invoker_free(Invoker *);
static VALUE invoker_call(int argc, VALUE* argv, VALUE self);
static VALUE invoker_call0(VALUE self);
static VALUE invoker_arity(VALUE self);
static void* callback_param(VALUE proc, VALUE cbinfo);
#ifdef USE_RAW
static void attached_method_invoke(ffi_cif* cif, void* retval, ffi_raw* parameters, void* user_data);
static void attached_method_vinvoke(ffi_cif* cif, void* retval, ffi_raw* parameters, void* user_data);
#else
#ifndef _WIN32
static void attached_method_invoke(ffi_cif* cif, void* retval, void** parameters, void* user_data);
#endif /* _WIN32 */
static void attached_method_vinvoke(ffi_cif* cif, void* retval, void** parameters, void* user_data);
#endif
static MethodHandle* method_handle_alloc(int arity);
static void method_handle_free(MethodHandle *);

static inline ThreadData* thread_data_get(void);

#ifndef _WIN32
static int PageSize;
#endif
static VALUE classInvoker = Qnil, classVariadicInvoker = Qnil;
static ID to_ptr = 0;
static ID map_symbol_id = 0;

#if defined(USE_PTHREAD_LOCAL)
static pthread_key_t threadDataKey;
#endif

#define threadData (thread_data_get())

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
#  define ADJ(p, a) ((p) = (FFIStorage*) (((char *) p) + a##_ADJ))
#else
#  define ADJ(p, a) (++(p))
#endif

static VALUE
invoker_allocate(VALUE klass)
{
    Invoker *invoker;
    return Data_Make_Struct(klass, Invoker, invoker_mark, invoker_free, invoker);
}

static VALUE
invoker_initialize(VALUE self, VALUE library, VALUE function, VALUE parameterTypes,
        VALUE rbReturnType, VALUE returnType, VALUE convention, VALUE enums)
{
    Invoker* invoker = NULL;
    ffi_type* ffiReturnType;
    ffi_abi abi;
    ffi_status ffiStatus;
    int i;

    Check_Type(parameterTypes, T_ARRAY);
    Check_Type(rbReturnType, T_SYMBOL);
    Check_Type(returnType, T_FIXNUM);
    Check_Type(convention, T_STRING);
    Check_Type(library, T_DATA);
    Check_Type(function, T_DATA);

    Data_Get_Struct(self, Invoker, invoker);
    invoker->enums = enums;
    invoker->library = library;
    invoker->function = rb_FFI_AbstractMemory_cast(function, rb_FFI_Pointer_class)->address;
    invoker->paramCount = RARRAY_LEN(parameterTypes);
    invoker->paramTypes = ALLOC_N(NativeType, invoker->paramCount);
    invoker->ffiParamTypes = ALLOC_N(ffi_type *, invoker->paramCount);

    for (i = 0; i < invoker->paramCount; ++i) {
        VALUE entry = rb_ary_entry(parameterTypes, i);
        if (rb_obj_is_kind_of(entry, rb_FFI_CallbackInfo_class)) {
            invoker->callbackParameters = REALLOC_N(invoker->callbackParameters, VALUE,
                    invoker->callbackCount + 1);
            invoker->callbackParameters[invoker->callbackCount++] = entry;
            invoker->paramTypes[i] = NATIVE_CALLBACK;
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
    invoker->rbReturnType = rbReturnType;
    invoker->returnType = FIX2INT(returnType);
    ffiReturnType = rb_FFI_NativeTypeToFFI(invoker->returnType);
    if (ffiReturnType == NULL) {
        rb_raise(rb_eArgError, "Invalid return type");
    }
#if defined(_WIN32) || defined(__WIN32__)
    abi = strcmp(StringValueCStr(convention), "stdcall") == 0 ? FFI_STDCALL : FFI_DEFAULT_ABI;
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
    invoker->methodHandle = method_handle_alloc(invoker->callbackCount < 1 ? invoker->paramCount : -1);
    invoker->methodHandle->invoker = invoker;
    return self;
}

static VALUE
variadic_invoker_new(VALUE klass, VALUE library, VALUE function, VALUE rbReturnType, VALUE returnType, VALUE convention, VALUE enums)
{
    Invoker* invoker = NULL;
    VALUE retval = Qnil;

    Check_Type(rbReturnType, T_SYMBOL);
    Check_Type(returnType, T_FIXNUM);
    Check_Type(convention, T_STRING);
    Check_Type(library, T_DATA);
    Check_Type(function, T_DATA);

    retval = Data_Make_Struct(klass, Invoker, invoker_mark, invoker_free, invoker);
    invoker->enums = enums;
    invoker->library = library;
    invoker->function = rb_FFI_AbstractMemory_cast(function, rb_FFI_Pointer_class)->address;
#if defined(_WIN32) || defined(__WIN32__)
    invoker->abi = strcmp(StringValueCStr(convention), "stdcall") == 0 ? FFI_STDCALL : FFI_DEFAULT_ABI;
#else
    invoker->abi = FFI_DEFAULT_ABI;
#endif
    invoker->rbReturnType = rbReturnType;
    invoker->returnType = FIX2INT(returnType);
    invoker->paramCount = -1;
    return retval;
}

static ffi_type* methodHandleParamTypes[MAX_FIXED_ARITY + 2];
static ffi_type* methodHandleVarargParamTypes[]= {
    &ffi_type_sint,
    &ffi_type_pointer,
    NULL,
};

#if defined(_WIN32) || defined(__WIN32__)
static MethodHandle*
method_handle_alloc(int arity)
{
    char errmsg[1024];
    ffi_type* ffiReturnType = (sizeof (VALUE) == sizeof (long))
            ? &ffi_type_ulong : &ffi_type_uint64;
    int ffiParamCount, ffiStatus;
    MethodHandle* method = NULL;
    ffi_type** ffiParamTypes;
    void (*fn)(ffi_cif* cif, void* retval, void** parameters, void* user_data);

    ffiParamCount = 3;
    ffiParamTypes = methodHandleVarargParamTypes;
    fn = attached_method_vinvoke;
    method = ALLOC_N(MethodHandle, 1);
    memset(method, 0, sizeof(*method));
    method->arity = -1;
    ffiStatus = ffi_prep_cif(&method->cif, FFI_DEFAULT_ABI, ffiParamCount,
            ffiReturnType, ffiParamTypes);
    if (ffiStatus != FFI_OK) {
        snprintf(errmsg, sizeof (errmsg), "ffi_prep_cif failed.  status=%#x", ffiStatus);
        goto error;
    }
    method->closure = ffi_closure_alloc(sizeof(*method->closure), &method->code);
    if (method->closure == NULL) {
        snprintf(errmsg, sizeof(errmsg), "ffi_closure_alloc failed");
        goto error;
    }
    ffiStatus = ffi_prep_closure_loc(method->closure, &method->cif, fn, method, method->code);
    if (ffiStatus != FFI_OK) {
        snprintf(errmsg, sizeof (errmsg), "ffi_prep_closure failed.  status=%#x", ffiStatus);
        goto error;
    }
    return method;
error:
    if (method != NULL) {
        if (method->closure != NULL) {
            ffi_closure_free(method->closure);
        }
        xfree(method);
    }
    rb_raise(rb_eRuntimeError, "%s", errmsg);
}
static void
method_handle_free(MethodHandle* method)
{
    if (method->closure != NULL) {
        ffi_closure_free(method->closure);
    }
    xfree(method);
}
#else
static MethodHandlePool methodHandlePool[MAX_FIXED_ARITY + 1], defaultMethodHandlePool;

#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32)
#  define pool_lock(p) pthread_mutex_lock(&(p)->mutex)
#  define pool_unlock(p)  pthread_mutex_unlock(&(p)->mutex)
#else
#  define pool_lock(p)
#  define pool_unlock(p)
#endif
static MethodHandle*
method_handle_alloc(int arity)
{
    MethodHandle* method, *list = NULL;
    MethodHandlePool* pool;
    ffi_type** ffiParamTypes;
    ffi_type* ffiReturnType;
    caddr_t page;
    int ffiParamCount, ffiStatus;
    int nclosures, closureSize, methodArity;
    int i;
#ifdef USE_RAW
    void (*fn)(ffi_cif* cif, void* retval, ffi_raw* parameters, void* user_data);
#else
    void (*fn)(ffi_cif* cif, void* retval, void** parameters, void* user_data);
#endif
    
    
    pool = (arity >= 0 && arity <= MAX_FIXED_ARITY) ? &methodHandlePool[arity] : &defaultMethodHandlePool;
    pool_lock(pool);
    if (pool->list != NULL) {
        method = pool->list;
        pool->list = pool->list->next;
        pool_unlock(pool);
        return method;
    }
    ffiReturnType = (sizeof (VALUE) == sizeof (long))
            ? &ffi_type_ulong : &ffi_type_uint64;
    closureSize = roundup(sizeof(METHOD_CLOSURE), 8);
    nclosures = PageSize / closureSize;
    page = mmap(NULL, PageSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (page == (caddr_t) -1) {
        pool_unlock(pool);
        rb_raise(rb_eRuntimeError, "mmap failed. errno=%d (%s)", errno, strerror(errno));
    }

    /* figure out whichh function to bounce the execution through */
    if (arity >= 0 && arity <= MAX_FIXED_ARITY) {
        ffiParamCount = arity + 1;
        ffiParamTypes = methodHandleParamTypes;
        fn = attached_method_invoke;
        methodArity = arity;
    } else {
        ffiParamCount = 3;
        ffiParamTypes = methodHandleVarargParamTypes;
        fn = attached_method_vinvoke;
        methodArity = -1;
    }

    for (i = 0; i < nclosures; ++i) {
        char errmsg[256];
        method = calloc(1, sizeof(MethodHandle));
        if (method == NULL) {
            snprintf(errmsg, sizeof(errmsg), "malloc failed: %s", strerror(errno));
            goto error;
        }
        method->next = list;
        list = method;
        method->pool = pool;
        method->code = method->closure = (METHOD_CLOSURE *) (page + (i * closureSize));

        ffiStatus = ffi_prep_cif(&method->cif, FFI_DEFAULT_ABI, ffiParamCount,
                    ffiReturnType, ffiParamTypes);
        if (ffiStatus != FFI_OK) {
            snprintf(errmsg, sizeof(errmsg), "ffi_prep_cif failed.  status=%#x", ffiStatus);
            goto error;
        }

#ifdef USE_RAW
        ffiStatus = ffi_prep_raw_closure(method->closure, &method->cif, fn, method);
#else
        ffiStatus = ffi_prep_closure(method->closure, &method->cif, fn, method);
#endif
        if (ffiStatus != FFI_OK) {
            snprintf(errmsg, sizeof(errmsg), "ffi_prep_closure failed.  status=%#x", ffiStatus);
            goto error;
        }
        method->arity = methodArity;
        continue;
error:
        while (list != NULL) {
            method = list;
            list = list->next;
            free(method);
        }
        munmap(page, PageSize);
        pool_unlock(pool);
        rb_raise(rb_eRuntimeError, "%s", errmsg);
    }
    mprotect(page, PageSize, PROT_READ | PROT_EXEC);

    /* take the first member of the list for this handle */
    method = list;
    list = list->next;

    /* now add the new block of MethodHandles to the pool */
    if (list != NULL) {
        list->next = pool->list;
        pool->list = list;
    }
    pool_unlock(pool);
    return method;
}
static void
method_handle_free(MethodHandle* method)
{
    MethodHandlePool* pool = method->pool;
    pool_lock(pool);
    method->next = pool->list;
    pool->list = method;
    pool_unlock(pool);
}
#endif /* _WIN32 */

typedef union {
#if BYTE_ORDER == LITTLE_ENDIAN
    signed int s8, s16, s32;
    unsigned int u8, u16, u32;
#else
    signed char s8;
    unsigned char u8;
    signed short s16;
    unsigned short u16;
    signed int s32;
    unsigned int u32;
#endif
    signed long long i64;
    unsigned long long u64;
    void* ptr;
    float f32;
    double f64;
} FFIStorage;

static inline int
getSignedInt(VALUE value, int type, int minValue, int maxValue, const char* typeName, VALUE enums)
{
    int i;
    if (type == T_SYMBOL && enums != Qnil) {
        value = rb_funcall(enums, map_symbol_id, 1, value);
        if (value == Qnil) {
            rb_raise(rb_eTypeError, "Expected a valid enum constant");
        }
    }
    else if (type != T_FIXNUM && type != T_BIGNUM) {
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
            case NATIVE_INT8:
                param->s8 = getSignedInt(argv[argidx++], type, -128, 127, "char", Qnil);
                ADJ(param, INT8);
                break;
            case NATIVE_INT16:
                param->s16 = getSignedInt(argv[argidx++], type, -0x8000, 0x7fff, "short", Qnil);
                ADJ(param, INT16);
                break;
            case NATIVE_INT32:
            case NATIVE_ENUM:
                param->s32 = getSignedInt(argv[argidx++], type, -0x80000000, 0x7fffffff, "int", invoker->enums);
                ADJ(param, INT32);
                break;
            case NATIVE_UINT8:
                param->u8 = getUnsignedInt(argv[argidx++], type, 0xff, "unsigned char");
                ADJ(param, INT8);
                break;
            case NATIVE_UINT16:
                param->u16 = getUnsignedInt(argv[argidx++], type, 0xffff, "unsigned short");
                ADJ(param, INT16);
                break;
            case NATIVE_UINT32:
                /* Special handling/checking for unsigned 32 bit integers */
                param->u32 = getUnsignedInt32(argv[argidx++], type);
                ADJ(param, INT32);
                break;
            case NATIVE_INT64:
                if (type != T_FIXNUM && type != T_BIGNUM) {
                    rb_raise(rb_eTypeError, "Expected an Integer parameter");
                }
                param->i64 = NUM2LL(argv[argidx]);
                ADJ(param, INT64);
                ++argidx;
                break;
            case NATIVE_UINT64:
                if (type != T_FIXNUM && type != T_BIGNUM) {
                    rb_raise(rb_eTypeError, "Expected an Integer parameter");
                }
                param->u64 = NUM2ULL(argv[argidx]);
                ADJ(param, INT64);
                ++argidx;
                break;
            case NATIVE_FLOAT32:
                if (type != T_FLOAT && type != T_FIXNUM) {
                    rb_raise(rb_eTypeError, "Expected a Float parameter");
                }
                param->f32 = (float) NUM2DBL(argv[argidx]);
                ADJ(param, FLOAT32);
                ++argidx;
                break;
            case NATIVE_FLOAT64:
                if (type != T_FLOAT && type != T_FIXNUM) {
                    rb_raise(rb_eTypeError, "Expected a Float parameter");
                }
                param->f64 = NUM2DBL(argv[argidx]);
                ADJ(param, FLOAT64);
                ++argidx;
                break;
            case NATIVE_STRING:
                if (type == T_STRING) {
                    if (rb_safe_level() >= 1 && OBJ_TAINTED(argv[argidx])) {
                        rb_raise(rb_eSecurityError, "Unsafe string parameter");
                    }
                    param->ptr = StringValueCStr(argv[argidx]);
                } else if (type == T_NIL) {
                    param->ptr = NULL;
                } else {
                    rb_raise(rb_eArgError, "Invalid String value");
                }
                ADJ(param, ADDRESS);
                ++argidx;
                break;
            case NATIVE_POINTER:
            case NATIVE_BUFFER_IN:
            case NATIVE_BUFFER_OUT:
            case NATIVE_BUFFER_INOUT:
                if (type == T_DATA && rb_obj_is_kind_of(argv[argidx], rb_FFI_AbstractMemory_class)) {
                    param->ptr = ((AbstractMemory *) DATA_PTR(argv[argidx]))->address;
                } else if (type == T_DATA && rb_obj_is_kind_of(argv[argidx], rb_FFI_Struct_class)) {
                    AbstractMemory* memory = ((Struct *) DATA_PTR(argv[argidx]))->pointer;
                    param->ptr = memory != NULL ? memory->address : NULL;
                } else if (type == T_STRING) {
                    if (rb_safe_level() >= 1 && OBJ_TAINTED(argv[argidx])) {
                        rb_raise(rb_eSecurityError, "Unsafe string parameter");
                    }
                    param->ptr = StringValuePtr(argv[argidx]);
                } else if (type == T_NIL) {
                    param->ptr = NULL;
                } else if (rb_respond_to(argv[argidx], to_ptr)) {
                    VALUE ptr = rb_funcall2(argv[argidx], to_ptr, 0, NULL);
                    if (rb_obj_is_kind_of(ptr, rb_FFI_AbstractMemory_class) && TYPE(ptr) == T_DATA) {
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
            case NATIVE_CALLBACK:
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
ffi_invoke(ffi_cif* cif, Invoker* invoker, void** ffiValues)
{
    FFIStorage retval;
    int error = 0;
    void* function = invoker->function;
    NativeType returnType = invoker->returnType;
    VALUE rbReturnType = invoker->rbReturnType;
    VALUE enums = invoker->enums;

#ifdef USE_RAW
    ffi_raw_call(cif, FFI_FN(function), &retval, (ffi_raw *) ffiValues[0]);
#else
    ffi_call(cif, FFI_FN(function), &retval, ffiValues);
#endif
#if defined(_WIN32) || defined(__WIN32__)
    error = GetLastError();
#else
    error = errno;
#endif
    threadData->td_errno = error;

    return rb_FFI_NativeValueToRuby(returnType, rbReturnType, &retval, enums);
}
static VALUE
invoker_call(int argc, VALUE* argv, VALUE self)
{
    Invoker* invoker;
    FFIStorage params[MAX_PARAMETERS];
    void* ffiValues[MAX_PARAMETERS];

    Data_Get_Struct(self, Invoker, invoker);
    ffi_arg_setup(invoker, argc, argv, invoker->paramTypes, params, ffiValues);
    return ffi_invoke(&invoker->cif, invoker, ffiValues);
}

static inline VALUE
invoker_callN(VALUE self, int argc, VALUE* argv)
{
    Invoker* invoker;
    FFIStorage params[3];
    void* ffiValues[3];

    Data_Get_Struct(self, Invoker, invoker);
    ffi_arg_setup(invoker, argc, argv, invoker->paramTypes, params, ffiValues);
    return ffi_invoke(&invoker->cif, invoker, ffiValues);
}

static VALUE
invoker_call0(VALUE self)
{
    Invoker* invoker;
    FFIStorage arg0;
    void* ffiValues[] = { &arg0 };
    
    Data_Get_Struct(self, Invoker, invoker);
    return ffi_invoke(&invoker->cif, invoker, ffiValues);
}

static VALUE
invoker_call1(VALUE self, VALUE arg1)
{
    return invoker_callN(self, 1, &arg1);
}

static VALUE
invoker_call2(VALUE self, VALUE arg1, VALUE arg2)
{
    VALUE argv[] = { arg1, arg2 };
    return invoker_callN(self, 2, argv);
}

static VALUE
invoker_call3(VALUE self, VALUE arg1, VALUE arg2, VALUE arg3)
{
    VALUE argv[] = { arg1, arg2, arg3 };
    return invoker_callN(self, 3, argv);
}

#ifdef USE_RAW
static void
attached_method_invoke(ffi_cif* cif, void* retval, ffi_raw* parameters, void* user_data)
{
    MethodHandle* handle =  (MethodHandle *) user_data;
    Invoker* invoker = handle->invoker;
    void* ffiValues[MAX_FIXED_ARITY];
    FFIStorage params[MAX_FIXED_ARITY];

    if (invoker->paramCount > 0) {
        ffi_arg_setup(invoker, invoker->paramCount, (VALUE *)&parameters[1],
                invoker->paramTypes, params, ffiValues);
    }
    *((VALUE *) retval) = ffi_invoke(&invoker->cif, invoker, ffiValues);
}

static void
attached_method_vinvoke(ffi_cif* cif, void* retval, ffi_raw* parameters, void* user_data)
{
    MethodHandle* handle =  (MethodHandle *) user_data;
    Invoker* invoker = handle->invoker;
    void** ffiValues = ALLOCA_N(void *, invoker->paramCount);
    FFIStorage* params = ALLOCA_N(FFIStorage, invoker->paramCount);
    int argc = parameters[0].sint;
    VALUE* argv = *(VALUE **) &parameters[1];

    ffi_arg_setup(invoker, argc, argv, invoker->paramTypes, params, ffiValues);
    *((VALUE *) retval) = ffi_invoke(&invoker->cif, invoker, ffiValues);
}

#else
#ifndef _WIN32
static void
attached_method_invoke(ffi_cif* cif, void* retval, void** parameters, void* user_data)
{
    MethodHandle* handle =  (MethodHandle *) user_data;
    Invoker* invoker = handle->invoker;
    void* ffiValues[MAX_FIXED_ARITY];
    FFIStorage params[MAX_FIXED_ARITY];
    VALUE argv[MAX_FIXED_ARITY];
    int i;
    //printf("Attached method invoke nargs=%d paramCount=%d\n", cif->nargs, invoker->paramCount); fflush(stdout);
    for (i = 0; i < invoker->paramCount; ++i) {
        memcpy(&argv[i], parameters[i + 1], sizeof(argv[i]));
    }
    if (invoker->paramCount > 0) {
        ffi_arg_setup(invoker, invoker->paramCount, argv, invoker->paramTypes, params, ffiValues);
    }
    *((VALUE *) retval) = ffi_invoke(&invoker->cif, invoker, ffiValues);
}
#endif /* _WIN32 */
static void
attached_method_vinvoke(ffi_cif* cif, void* retval, void** parameters, void* user_data)
{
    MethodHandle* handle =  (MethodHandle *) user_data;
    Invoker* invoker = handle->invoker;
    void** ffiValues = ALLOCA_N(void *, invoker->paramCount);
    FFIStorage* params = ALLOCA_N(FFIStorage, invoker->paramCount);
    int argc = *(int *) parameters[0];
    VALUE* argv = *(VALUE **) parameters[1];

    ffi_arg_setup(invoker, argc, argv, invoker->paramTypes, params, ffiValues);
    *((VALUE *) retval) = ffi_invoke(&invoker->cif, invoker, ffiValues);
}
#endif
static VALUE
invoker_attach(VALUE self, VALUE module, VALUE name)
{
    Invoker* invoker;
    MethodHandle* handle;
    char var[1024];
    Data_Get_Struct(self, Invoker, invoker);
    handle = invoker->methodHandle;
    rb_define_module_function(module, StringValuePtr(name), 
            handle->code, handle->arity);
    snprintf(var, sizeof(var), "@@%s", StringValueCStr(name));
    rb_cv_set(module, var, self);
    return self;
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
    if (invoker->enums != Qnil) {
        rb_gc_mark(invoker->enums);
    }
    if (invoker->rbReturnType != Qnil) {
        rb_gc_mark(invoker->rbReturnType);
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
        if (invoker->methodHandle != NULL) {
            method_handle_free(invoker->methodHandle);
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
            case NATIVE_INT8:
            case NATIVE_INT16:
            case NATIVE_INT32:
            case NATIVE_ENUM:
                paramType = NATIVE_INT32;
                break;
            case NATIVE_UINT8:
            case NATIVE_UINT16:
            case NATIVE_UINT32:
                paramType = NATIVE_UINT32;
                break;
            case NATIVE_FLOAT32:
                paramType = NATIVE_FLOAT64;
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
    return ffi_invoke(&cif, invoker, ffiValues);
}

static void* 
callback_param(VALUE proc, VALUE cbInfo)
{
    VALUE callback ;
    if (proc == Qnil) {
        return NULL ;
    }
    callback = rb_FFI_NativeCallback_for_proc(proc, cbInfo);
    return ((NativeCallback *) DATA_PTR(callback))->code;
}

#if defined(USE_PTHREAD_LOCAL)
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

#else
static ID thread_data_id;

static ThreadData*
thread_data_init()
{
    ThreadData* td;
    VALUE obj;
    obj = Data_Make_Struct(rb_cObject, ThreadData, NULL, -1, td);
    rb_thread_local_aset(rb_thread_current(), thread_data_id, obj);
    return td;
}

static inline ThreadData*
thread_data_get()
{
    VALUE obj = rb_thread_local_aref(rb_thread_current(), thread_data_id);

    if (obj != Qnil && TYPE(obj) == T_DATA) {
        return (ThreadData *) DATA_PTR(obj);
    }
    return thread_data_init();
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
    ffi_type* ffiValueType;
    int i;
    VALUE moduleFFI = rb_define_module("FFI");
    VALUE moduleError = rb_define_module_under(moduleFFI, "LastError");
    classInvoker = rb_define_class_under(moduleFFI, "Invoker", rb_cObject);
    rb_define_alloc_func(classInvoker, invoker_allocate);
    rb_define_method(classInvoker, "initialize", invoker_initialize, 7);
    rb_define_method(classInvoker, "call", invoker_call, -1);
    rb_define_method(classInvoker, "call0", invoker_call0, 0);
    rb_define_method(classInvoker, "call1", invoker_call1, 1);
    rb_define_method(classInvoker, "call2", invoker_call2, 2);
    rb_define_method(classInvoker, "call3", invoker_call3, 3);
    rb_define_method(classInvoker, "arity", invoker_arity, 0);
    rb_define_method(classInvoker, "attach", invoker_attach, 2);
    classVariadicInvoker = rb_define_class_under(moduleFFI, "VariadicInvoker", rb_cObject);
    rb_define_singleton_method(classVariadicInvoker, "__new", variadic_invoker_new, 6);
    rb_define_method(classVariadicInvoker, "invoke", variadic_invoker_call, 2);
    to_ptr = rb_intern("to_ptr");
    map_symbol_id = rb_intern("__map_symbol");
    
    rb_define_module_function(moduleError, "error", get_last_error, 0);
    rb_define_module_function(moduleError, "error=", set_last_error, 1);

    ffiValueType = (sizeof (VALUE) == sizeof (long))
            ? &ffi_type_ulong : &ffi_type_uint64;
    for (i = 0; i <= MAX_FIXED_ARITY + 1; ++i) {
        methodHandleParamTypes[i] = ffiValueType;
    }
    methodHandleVarargParamTypes[2] = ffiValueType;

#ifndef _WIN32
    PageSize = sysconf(_SC_PAGESIZE);
#endif /* _WIN32 */

#if defined(USE_PTHREAD_LOCAL)
    pthread_key_create(&threadDataKey, thread_data_free);
    for (i = 0; i < 4; ++i) {
        pthread_mutex_init(&methodHandlePool[i].mutex, NULL);
    }
    pthread_mutex_init(&defaultMethodHandlePool.mutex, NULL);
#else
    thread_data_id = rb_intern("ffi_thread_local_data");
#endif /* USE_PTHREAD_LOCAL */

}
