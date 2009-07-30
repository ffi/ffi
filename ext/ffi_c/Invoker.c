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
#include "Call.h"


#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32) && !defined(__WIN32__)
#  define USE_PTHREAD_LOCAL
#endif


#ifndef roundup
#  define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif

typedef struct MethodHandle MethodHandle;
typedef struct Invoker Invoker;

struct Invoker {
    VALUE address;
    VALUE enums;
    void* function;
    
    ffi_cif cif;
    int paramCount;
    ffi_type** ffiParamTypes;
    NativeType* paramTypes;
    Type* returnType;
    VALUE rbReturnType;
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

static VALUE invoker_allocate(VALUE klass);
static VALUE invoker_initialize(VALUE self, VALUE function, VALUE parameterTypes,
        VALUE returnType, VALUE convention, VALUE enums);
static void invoker_mark(Invoker *);
static void invoker_free(Invoker *);
static VALUE invoker_call(int argc, VALUE* argv, VALUE self);
static VALUE invoker_arity(VALUE self);

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


#ifndef _WIN32
static int PageSize;
#endif

VALUE rbffi_InvokerClass = Qnil;
static VALUE classInvoker = Qnil, classVariadicInvoker = Qnil;

static VALUE
invoker_allocate(VALUE klass)
{
    Invoker *invoker;
    VALUE obj = Data_Make_Struct(klass, Invoker, invoker_mark, invoker_free, invoker);

    invoker->rbReturnType = Qnil;
    invoker->address = Qnil;
    invoker->enums = Qnil;

    return obj;
}

static VALUE
invoker_initialize(VALUE self, VALUE function, VALUE parameterTypes,
       VALUE returnType, VALUE convention, VALUE enums)
{
    Invoker* invoker = NULL;
    ffi_type* ffiReturnType;
    ffi_abi abi;
    ffi_status ffiStatus;
    int i;

    Check_Type(parameterTypes, T_ARRAY);
    Check_Type(convention, T_STRING);
    Check_Type(function, T_DATA);

    Data_Get_Struct(self, Invoker, invoker);
    invoker->enums = enums;
    invoker->address = function;
    invoker->function = rbffi_AbstractMemory_Cast(function, rbffi_PointerClass)->address;
    invoker->paramCount = RARRAY_LEN(parameterTypes);
    invoker->paramTypes = ALLOC_N(NativeType, invoker->paramCount);
    invoker->ffiParamTypes = ALLOC_N(ffi_type *, invoker->paramCount);

    for (i = 0; i < invoker->paramCount; ++i) {
        VALUE entry = rb_ary_entry(parameterTypes, i);
        VALUE rbType = rbffi_Type_Lookup(entry);
        Type* type;

        if (!RTEST(rbType)) {
            VALUE typeName = rb_funcall2(entry, rb_intern("inspect"), 0, NULL);
            rb_raise(rb_eTypeError, "Invalid parameter type (%s)", RSTRING_PTR(typeName));
        }

        Data_Get_Struct(entry, Type, type);

        if (rb_obj_is_kind_of(rbType, rbffi_FunctionInfoClass)) {
            invoker->callbackParameters = REALLOC_N(invoker->callbackParameters, VALUE,
                    invoker->callbackCount + 1);
            invoker->callbackParameters[invoker->callbackCount++] = entry;
        }

        invoker->paramTypes[i] = type->nativeType;
        invoker->ffiParamTypes[i] = type->ffiType;
    }

    if (!rb_obj_is_kind_of(returnType, rbffi_TypeClass)) {
        rb_raise(rb_eTypeError, "Invalid return type");
    }
    
    invoker->rbReturnType = returnType;
    Data_Get_Struct(returnType, Type, invoker->returnType);

    ffiReturnType = invoker->returnType->ffiType;
    if (ffiReturnType == NULL) {
        rb_raise(rb_eTypeError, "Invalid return type");
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
variadic_invoker_initialize(VALUE self, VALUE function, VALUE parameterTypes, VALUE returnType, VALUE options)
{
    Invoker* invoker = NULL;
    VALUE retval = Qnil;
    VALUE convention = Qnil;
    VALUE fixed = Qnil;
    int i;

    Check_Type(function, T_DATA);
    Check_Type(options, T_HASH);
    convention = rb_hash_aref(options, ID2SYM(rb_intern("convention")));

    Data_Get_Struct(self, Invoker, invoker);
    invoker->enums = rb_hash_aref(options, ID2SYM(rb_intern("enums")));
    invoker->address = function;
    invoker->function = rbffi_AbstractMemory_Cast(function, rbffi_PointerClass)->address;

#if defined(_WIN32) || defined(__WIN32__)
    invoker->abi = (RTEST(convention) && strcmp(StringValueCStr(convention), "stdcall") == 0)
            ? FFI_STDCALL : FFI_DEFAULT_ABI;
#else
    invoker->abi = FFI_DEFAULT_ABI;
#endif

    invoker->rbReturnType = rbffi_Type_Lookup(returnType);
    if (!RTEST(invoker->rbReturnType)) {
        VALUE typeName = rb_funcall2(returnType, rb_intern("inspect"), 0, NULL);
        rb_raise(rb_eTypeError, "Invalid return type (%s)", RSTRING_PTR(typeName));
    }

    Data_Get_Struct(returnType, Type, invoker->returnType);

    invoker->paramCount = -1;

    fixed = rb_ary_new2(RARRAY_LEN(parameterTypes) - 1);
    for (i = 0; i < RARRAY_LEN(parameterTypes); ++i) {
        VALUE entry = rb_ary_entry(parameterTypes, i);
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

static inline VALUE
ffi_invoke(ffi_cif* cif, Invoker* invoker, void** ffiValues)
{
    FFIStorage retval;

#ifdef USE_RAW
    ffi_raw_call(cif, FFI_FN(invoker->function), &retval, (ffi_raw *) ffiValues[0]);
#else
    ffi_call(cif, FFI_FN(invoker->function), &retval, ffiValues);
#endif
    rbffi_save_errno();

    return rbffi_NativeValue_ToRuby(invoker->returnType, invoker->rbReturnType, &retval,
        invoker->enums);
}

static VALUE
invoker_call(int argc, VALUE* argv, VALUE self)
{
    Invoker* invoker;
    int argCount;
    FFIStorage params_[MAX_FIXED_ARITY], *params = &params_[0];
    void* ffiValues_[MAX_FIXED_ARITY], **ffiValues = &ffiValues_[0];

    Data_Get_Struct(self, Invoker, invoker);

    argCount = invoker->paramCount != -1 ? invoker->paramCount : argc;

    if (argCount > MAX_FIXED_ARITY) {
        params = ALLOCA_N(FFIStorage, argCount);
        ffiValues = ALLOCA_N(void *, argCount);
    }

    rbffi_SetupCallParams(argc, argv, invoker->paramCount, invoker->paramTypes, params, ffiValues,
        invoker->callbackParameters, invoker->callbackCount, invoker->enums);

    return ffi_invoke(&invoker->cif, invoker, ffiValues);
}

#ifdef USE_RAW

/*
 * attached_method_invoke is used as the <= 3 argument fixed-arity fast path
 */
static void
attached_method_invoke(ffi_cif* cif, void* retval, ffi_raw* parameters, void* user_data)
{
    MethodHandle* handle =  (MethodHandle *) user_data;
    Invoker* invoker = handle->invoker;
    void* ffiValues[MAX_FIXED_ARITY];
    FFIStorage params[MAX_FIXED_ARITY];

    if (invoker->paramCount > 0) {
        rbffi_SetupCallParams(invoker->paramCount, (VALUE *)&parameters[1],
                invoker->paramCount, invoker->paramTypes, params, ffiValues,
                invoker->callbackParameters, invoker->callbackCount, invoker->enums);
    }
    *((VALUE *) retval) = ffi_invoke(&invoker->cif, invoker, ffiValues);
}

/*
 * attached_method_vinvoke is used functions with more than 3 parameters
 */
static void
attached_method_vinvoke(ffi_cif* cif, void* retval, ffi_raw* parameters, void* user_data)
{
    MethodHandle* handle =  (MethodHandle *) user_data;
    Invoker* invoker = handle->invoker;
    void** ffiValues = ALLOCA_N(void *, invoker->paramCount);
    FFIStorage* params = ALLOCA_N(FFIStorage, invoker->paramCount);
    int argc = parameters[0].sint;
    VALUE* argv = *(VALUE **) &parameters[1];

    rbffi_SetupCallParams(argc, argv, invoker->paramCount, invoker->paramTypes, params, ffiValues,
        invoker->callbackParameters, invoker->callbackCount, invoker->enums);
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
        rbffi_SetupCallParams(invoker->paramCount, argv, invoker->paramCount, invoker->paramTypes,
                params, ffiValues, invoker->callbackParameters, invoker->callbackCount,
                invoker->enums);
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

    rbffi_SetupCallParams(argc, argv, invoker->paramCount, invoker->paramTypes, params,
        ffiValues, invoker->callbackParameters, invoker->callbackCount,
        invoker->enums);
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
    if (invoker->paramCount == -1) {
        rb_raise(rb_eRuntimeError, "Cannot attach variadic invokers");
    }

    handle = invoker->methodHandle;
    rb_define_module_function(module, StringValueCStr(name), 
            handle->code, handle->arity);
    snprintf(var, sizeof(var), "@@%s", StringValueCStr(name));
    rb_cv_set(module, var, self);
    rb_define_method(module, StringValueCStr(name), 
            handle->code, handle->arity);
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
    if (invoker->address != Qnil) {
        rb_gc_mark(invoker->address);
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
    rbffi_SetupCallParams(paramCount, argv, invoker->paramCount, paramTypes, params,
        ffiValues, invoker->callbackParameters, invoker->callbackCount,
        invoker->enums);
    return ffi_invoke(&cif, invoker, ffiValues);
}


void 
rbffi_Invoker_Init(VALUE moduleFFI)
{
    ffi_type* ffiValueType;
    int i;
    
    rbffi_InvokerClass = classInvoker = rb_define_class_under(moduleFFI, "Invoker", rb_cObject);
    rb_global_variable(&rbffi_InvokerClass);
    rb_global_variable(&classInvoker);

    rb_define_alloc_func(classInvoker, invoker_allocate);
    rb_define_method(classInvoker, "initialize", invoker_initialize, 5);
    rb_define_method(classInvoker, "call", invoker_call, -1);
    rb_define_alias(classInvoker, "call0", "call");
    rb_define_alias(classInvoker, "call1", "call");
    rb_define_alias(classInvoker, "call2", "call");
    rb_define_alias(classInvoker, "call3", "call");
    rb_define_method(classInvoker, "arity", invoker_arity, 0);
    rb_define_method(classInvoker, "attach", invoker_attach, 2);
    classVariadicInvoker = rb_define_class_under(moduleFFI, "VariadicInvoker", rb_cObject);
    rb_global_variable(&classVariadicInvoker);

    rb_define_alloc_func(classVariadicInvoker, invoker_allocate);

    rb_define_method(classVariadicInvoker, "initialize", variadic_invoker_initialize, 4);
    rb_define_method(classVariadicInvoker, "invoke", variadic_invoker_call, 2);
    rb_define_alias(classVariadicInvoker, "call", "invoke");
    
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
    for (i = 0; i < 4; ++i) {
        pthread_mutex_init(&methodHandlePool[i].mutex, NULL);
    }
    pthread_mutex_init(&defaultMethodHandlePool.mutex, NULL);
#endif /* USE_PTHREAD_LOCAL */

}
