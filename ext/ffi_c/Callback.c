#include <sys/param.h>
#include <sys/types.h>
#ifndef _WIN32
# include <sys/mman.h>
#endif
#include <ruby.h>
#include <ffi.h>
#include "AbstractMemory.h"
#include "Pointer.h"
#include "MemoryPointer.h"
#include "Callback.h"
#include "Types.h"
#include "Type.h"
#include "rbffi.h"
#include "compat.h"
#include "extconf.h"

static void cbinfo_mark(CallbackInfo* cbInfo);
static void cbinfo_free(CallbackInfo *);

#if defined(HAVE_LIBFFI) && !defined(HAVE_FFI_CLOSURE_ALLOC)
static void* ffi_closure_alloc(size_t size, void** code);
static void ffi_closure_free(void* ptr);
ffi_status ffi_prep_closure_loc(ffi_closure* closure, ffi_cif* cif,
        void (*fun)(ffi_cif*, void*, void**, void*),
        void* user_data, void* code);
#endif /* HAVE_FFI_CLOSURE_ALLOC */

static VALUE CallbackInfoClass = Qnil;
static VALUE NativeCallbackClass = Qnil;
static ID id_call = Qnil, id_cbtable = Qnil;

VALUE rbffi_CallbackInfoClass = Qnil;

static VALUE
cbinfo_allocate(VALUE klass)
{
    CallbackInfo* cbInfo;
    VALUE obj = Data_Make_Struct(klass, CallbackInfo, cbinfo_mark, cbinfo_free, cbInfo);

    cbInfo->type.nativeType = NATIVE_CALLBACK;
    cbInfo->rbReturnType = Qnil;

    return obj;
}

static VALUE
cbinfo_initialize(VALUE self, VALUE rbReturnType, VALUE rbParamTypes)
{
    CallbackInfo *cbInfo;
    int paramCount;
    ffi_status status;
    int i;

    Check_Type(rbParamTypes, T_ARRAY);
    paramCount = RARRAY_LEN(rbParamTypes);

    Data_Get_Struct(self, CallbackInfo, cbInfo);
    cbInfo->parameterCount = paramCount;
    cbInfo->parameterTypes = xcalloc(paramCount, sizeof(NativeType));
    cbInfo->ffiParameterTypes = xcalloc(paramCount, sizeof(ffi_type *));
    cbInfo->returnType = rbffi_Type_GetIntValue(rbReturnType);
    cbInfo->rbReturnType = rbReturnType;
    cbInfo->rbParameterTypes = rbParamTypes;

    for (i = 0; i < paramCount; ++i) {
        cbInfo->parameterTypes[i] = rbffi_Type_GetIntValue(rb_ary_entry(rbParamTypes, i));
        cbInfo->ffiParameterTypes[i] = rbffi_NativeType_ToFFI(cbInfo->parameterTypes[i]);
        if (cbInfo->ffiParameterTypes[i] == NULL) {
            rb_raise(rb_eArgError, "Unknown argument type: %#x", cbInfo->parameterTypes[i]);
        }
    }

    cbInfo->ffiReturnType = rbffi_NativeType_ToFFI(cbInfo->returnType);
    if (cbInfo->ffiReturnType == NULL) {
        rb_raise(rb_eArgError, "Unknown return type: %#x", cbInfo->returnType);
    }
#if defined(_WIN32) && defined(notyet)
    cbInfo->abi = (flags & STDCALL) ? FFI_STDCALL : FFI_DEFAULT_ABI;
#else
    cbInfo->abi = FFI_DEFAULT_ABI;
#endif
    status = ffi_prep_cif(&cbInfo->ffi_cif, cbInfo->abi, cbInfo->parameterCount, 
            cbInfo->ffiReturnType, cbInfo->ffiParameterTypes);
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

static void
cbinfo_mark(CallbackInfo* cbInfo)
{
    rb_gc_mark(cbInfo->rbReturnType);
    rb_gc_mark(cbInfo->rbParameterTypes);
}

static void
cbinfo_free(CallbackInfo* cbInfo)
{
    if (cbInfo->parameterTypes != NULL) {
        xfree(cbInfo->parameterTypes);
    }
    if (cbInfo->ffiParameterTypes != NULL) {
        xfree(cbInfo->ffiParameterTypes);
    }
    xfree(cbInfo);
}

static void
native_callback_free(NativeCallback* cb)
{
    if (cb->ffi_closure != NULL) {
        ffi_closure_free(cb->ffi_closure);
    }
    xfree(cb);
}

static void
native_callback_mark(NativeCallback* cb)
{
    rb_gc_mark(cb->rbCallbackInfo);
    rb_gc_mark(cb->rbProc);
}

static void
native_callback_invoke(ffi_cif* cif, void* retval, void** parameters, void* user_data)
{
    NativeCallback* cb = (NativeCallback *) user_data;
    CallbackInfo *cbInfo = cb->cbInfo;
    VALUE* rbParams;
    VALUE rbReturnValue;
    int i;

    rbParams = ALLOCA_N(VALUE, cbInfo->parameterCount);
    for (i = 0; i < cbInfo->parameterCount; ++i) {
        VALUE param;
        switch (cbInfo->parameterTypes[i]) {
            case NATIVE_INT8:
                param = INT2NUM(*(int8_t *) parameters[i]);
                break;
            case NATIVE_UINT8:
                param = UINT2NUM(*(uint8_t *) parameters[i]);
                break;
            case NATIVE_INT16:
                param = INT2NUM(*(int16_t *) parameters[i]);
                break;
            case NATIVE_UINT16:
                param = UINT2NUM(*(uint16_t *) parameters[i]);
                break;
            case NATIVE_INT32:
                param = INT2NUM(*(int32_t *) parameters[i]);
                break;
            case NATIVE_UINT32:
                param = UINT2NUM(*(uint32_t *) parameters[i]);
                break;
            case NATIVE_INT64:
                param = LL2NUM(*(int64_t *) parameters[i]);
                break;
            case NATIVE_UINT64:
                param = ULL2NUM(*(uint64_t *) parameters[i]);
                break;
            case NATIVE_FLOAT32:
                param = rb_float_new(*(float *) parameters[i]);
                break;
            case NATIVE_FLOAT64:
                param = rb_float_new(*(double *) parameters[i]);
                break;
            case NATIVE_STRING:
                param = rb_tainted_str_new2(*(char **) parameters[i]);
                break;
            case NATIVE_POINTER:
                param = rbffi_Pointer_NewInstance(*(void **) parameters[i]);
                break;
            case NATIVE_CALLBACK:
                param = rbffi_NativeValue_ToRuby(NATIVE_CALLBACK,
                     rb_ary_entry(cbInfo->rbParameterTypes, i), parameters[i], Qnil);
                break;
            default:
                param = Qnil;
                break;
        }
        rbParams[i] = param;
    }
    rbReturnValue = rb_funcall2(cb->rbProc, id_call, cbInfo->parameterCount, rbParams);
    if (rbReturnValue == Qnil || TYPE(rbReturnValue) == T_NIL) {
        memset(retval, 0, cbInfo->ffiReturnType->size);
    } else switch (cbInfo->returnType) {
        case NATIVE_INT8:
        case NATIVE_INT16:
        case NATIVE_INT32:
            *((ffi_sarg *) retval) = NUM2INT(rbReturnValue);
            break;
        case NATIVE_UINT8:
        case NATIVE_UINT16:
        case NATIVE_UINT32:
            *((ffi_arg *) retval) = NUM2UINT(rbReturnValue);
            break;
        case NATIVE_INT64:
            *((int64_t *) retval) = NUM2LL(rbReturnValue);
            break;
        case NATIVE_UINT64:
            *((uint64_t *) retval) = NUM2ULL(rbReturnValue);
            break;
        case NATIVE_FLOAT32:
            *((float *) retval) = (float) NUM2DBL(rbReturnValue);
            break;
        case NATIVE_FLOAT64:
            *((double *) retval) = NUM2DBL(rbReturnValue);
            break;
        case NATIVE_POINTER:
            if (TYPE(rbReturnValue) == T_DATA && rb_obj_is_kind_of(rbReturnValue, rbffi_PointerClass)) {
                *((void **) retval) = ((AbstractMemory *) DATA_PTR(rbReturnValue))->address;
            } else {
                // Default to returning NULL if not a value pointer object.  handles nil case as well
                *((void **) retval) = NULL;
            }
            break;

        case NATIVE_CALLBACK:
            if (rb_obj_is_kind_of(rbReturnValue, rb_cProc) || rb_respond_to(rbReturnValue, id_call)) {
                VALUE callback;

                callback = rbffi_NativeCallback_ForProc(rbReturnValue, cbInfo->rbReturnType);
                
                *((void **) retval) = ((NativeCallback *) DATA_PTR(callback))->code;
            } else {
                *((void **) retval) = NULL;
            }
            break;

        default:
            *((ffi_arg *) retval) = 0;
            break;
    }
}

static VALUE
native_callback_allocate(VALUE klass)
{
    NativeCallback* closure;
    VALUE obj;

    obj = Data_Make_Struct(klass, NativeCallback, native_callback_mark, native_callback_free, closure);
    closure->rbCallbackInfo = Qnil;
    closure->rbProc = Qnil;
    
    return obj;
}

VALUE
rbffi_NativeCallback_NewInstance(VALUE rbCallbackInfo, VALUE rbProc)
{
    NativeCallback* closure = NULL;
    CallbackInfo* cbInfo;
    VALUE obj;
    ffi_status status;

    Data_Get_Struct(rbCallbackInfo, CallbackInfo, cbInfo);
    obj = Data_Make_Struct(NativeCallbackClass, NativeCallback, native_callback_mark, native_callback_free, closure);
    closure->cbInfo = cbInfo;
    closure->rbProc = rbProc;
    closure->rbCallbackInfo = rbCallbackInfo;

    closure->ffi_closure = ffi_closure_alloc(sizeof(*closure->ffi_closure), &closure->code);
    if (closure->ffi_closure == NULL) {
        rb_raise(rb_eNoMemError, "Failed to allocate FFI native closure");
    }

    status = ffi_prep_closure_loc(closure->ffi_closure, &cbInfo->ffi_cif,
            native_callback_invoke, closure, closure->code);
    if (status != FFI_OK) {
        rb_raise(rb_eArgError, "ffi_prep_closure_loc failed");
    }

    return obj;
}

VALUE
rbffi_NativeCallback_ForProc(VALUE proc, VALUE cbInfo)
{
    VALUE callback;
    VALUE cbTable = RTEST(rb_ivar_defined(proc, id_cbtable)) ? rb_ivar_get(proc, id_cbtable) : Qnil;
    if (cbTable == Qnil) {
        cbTable = rb_hash_new();
        rb_ivar_set(proc, id_cbtable, cbTable);
    }
    callback = rb_hash_aref(cbTable, cbInfo);
    if (callback != Qnil) {
        return callback;
    }
    callback = rbffi_NativeCallback_NewInstance(cbInfo, proc);
    rb_hash_aset(cbTable, cbInfo, callback);
    return callback;
}
#if defined(HAVE_LIBFFI) && !defined(HAVE_FFI_CLOSURE_ALLOC)
/*
 * versions of ffi_closure_alloc, ffi_closure_free and ffi_prep_closure_loc for older
 * system libffi versions.
 */
static void*
ffi_closure_alloc(size_t size, void** code)
{
    void* closure;
    closure = mmap(NULL, size, PROT_READ | PROT_WRITE,
             MAP_ANON | MAP_PRIVATE, -1, 0);
    if (closure == (void *) -1) {
        return NULL;
    }
    memset(closure, 0, size);
    *code = closure;
    return closure;
}

static void
ffi_closure_free(void* ptr)
{
    munmap(ptr, sizeof(ffi_closure));
}

ffi_status
ffi_prep_closure_loc(ffi_closure* closure, ffi_cif* cif,
        void (*fun)(ffi_cif*, void*, void**, void*),
        void* user_data, void* code)
{
    ffi_status retval = ffi_prep_closure(closure, cif, fun, user_data);
    if (retval == FFI_OK) {
        mprotect(closure, sizeof(ffi_closure), PROT_READ | PROT_EXEC);
    }
    return retval;
}

#endif /* HAVE_FFI_CLOSURE_ALLOC */

void
rbffi_Callback_Init(VALUE moduleFFI)
{
    rbffi_CallbackInfoClass = CallbackInfoClass = rb_define_class_under(moduleFFI, "CallbackInfo", rbffi_TypeClass);
    rb_global_variable(&rbffi_CallbackInfoClass);

    rb_define_alloc_func(CallbackInfoClass, cbinfo_allocate);
    rb_define_method(CallbackInfoClass, "initialize", cbinfo_initialize, 2);

    NativeCallbackClass = rb_define_class_under(moduleFFI, "NativeCallback", rb_cObject);
    rb_global_variable(&NativeCallbackClass);
    rb_define_alloc_func(NativeCallbackClass, native_callback_allocate);
    id_call = rb_intern("call");
    id_cbtable = rb_intern("@__ffi_callback_table__");
}
