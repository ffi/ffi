#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <ruby.h>
#include <ffi.h>
#include "AbstractMemory.h"
#include "Pointer.h"
#include "MemoryPointer.h"
#include "Callback.h"
#include "Types.h"
#include "rbffi.h"
#include "extconf.h"


static void callback_mark(CallbackInfo *);
static void callback_free(CallbackInfo *);

#if defined(HAVE_LIBFFI) && !defined(HAVE_FFI_CLOSURE_ALLOC)
static void* ffi_closure_alloc(size_t size, void** code);
static void ffi_closure_free(void* ptr);
ffi_status ffi_prep_closure_loc(ffi_closure* closure, ffi_cif* cif,
        void (*fun)(ffi_cif*, void*, void**, void*),
        void* user_data, void* code);
#endif /* HAVE_FFI_CLOSURE_ALLOC */

static VALUE classCallback = Qnil;
static VALUE classNativeCallback = Qnil;
static ID callID = Qnil;

VALUE rb_FFI_Callback_class = Qnil;

static VALUE
callback_new(VALUE klass, VALUE rbReturnType, VALUE rbParamTypes)
{
    CallbackInfo *cbInfo;
    VALUE retval;
    int paramCount = RARRAY_LEN(rbParamTypes);
    ffi_status status;
    int i;

    retval = Data_Make_Struct(klass, CallbackInfo, callback_mark, callback_free, cbInfo);
    cbInfo->parameterCount = paramCount;
    cbInfo->parameterTypes = xcalloc(paramCount, sizeof(NativeType));
    cbInfo->ffiParameterTypes = xcalloc(paramCount, sizeof(ffi_type *));
    for (i = 0; i < paramCount; ++i) {
        cbInfo->parameterTypes[i] = FIX2INT(rb_ary_entry(rbParamTypes, i));
        cbInfo->ffiParameterTypes[i] = rb_FFI_NativeTypeToFFI(cbInfo->parameterTypes[i]);
        if (cbInfo->ffiParameterTypes[i] == NULL) {
            rb_raise(rb_eArgError, "Unknown argument type: %#x", cbInfo->parameterTypes[i]);
        }
    }
    cbInfo->returnType = FIX2INT(rbReturnType);
    cbInfo->ffiReturnType = rb_FFI_NativeTypeToFFI(cbInfo->returnType);
    if (cbInfo->ffiReturnType == NULL) {
        rb_raise(rb_eArgError, "Unknown return type: %#x", cbInfo->returnType);
    }
#ifdef _WIN32
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
    return retval;
}

static void
callback_mark(CallbackInfo* cbinfo)
{
}

static void
callback_free(CallbackInfo* cbInfo)
{
    if (cbInfo != NULL) {
        if (cbInfo->parameterTypes != NULL) {
            xfree(cbInfo->parameterTypes);
            cbInfo->parameterTypes = NULL;
        }
        if (cbInfo->ffiParameterTypes != NULL) {
            xfree(cbInfo->ffiParameterTypes);
            cbInfo->ffiParameterTypes = NULL;
        }
        xfree(cbInfo);
    }
}

static void
native_callback_free(NativeCallback* cb)
{
    if (cb != NULL) {
        if (cb->ffi_closure != NULL) {
            ffi_closure_free(cb->ffi_closure);
        }
    }
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
            case INT8:
                param = INT2NUM(*(int8_t *) parameters[i]);
                break;
            case UINT8:
                param = UINT2NUM(*(u_int8_t *) parameters[i]);
                break;
            case INT16:
                param = INT2NUM(*(int16_t *) parameters[i]);
                break;
            case UINT16:
                param = UINT2NUM(*(u_int16_t *) parameters[i]);
                break;
            case INT32:
                param = INT2NUM(*(int32_t *) parameters[i]);
                break;
            case UINT32:
                param = UINT2NUM(*(u_int32_t *) parameters[i]);
                break;
            case INT64:
                param = LL2NUM(*(int64_t *) parameters[i]);
                break;
            case UINT64:
                param = ULL2NUM(*(u_int64_t *) parameters[i]);
                break;
            case FLOAT32:
                param = rb_float_new(*(float *) parameters[i]);
                break;
            case FLOAT64:
                param = rb_float_new(*(double *) parameters[i]);
                break;
            case STRING:
                param = rb_tainted_str_new2(*(char **) parameters[i]);
                break;
            case POINTER:
                param = rb_FFI_Pointer_new(*(caddr_t *) parameters[i]);
                break;
            default:
                param = Qnil;
                break;
        }
        rbParams[i] = param;
    }
    rbReturnValue = rb_funcall2(cb->rbProc, callID, cbInfo->parameterCount, rbParams);
    if (rbReturnValue == Qnil || TYPE(rbReturnValue) == T_NIL) {
        memset(retval, 0, cbInfo->ffiReturnType->size);
    } else switch (cbInfo->returnType) {
        case INT8:
        case INT16:
        case INT32:
            *((long *) retval) = NUM2INT(rbReturnValue);
            break;
        case UINT8:
        case UINT16:
        case UINT32:
            *((unsigned long *) retval) = NUM2UINT(rbReturnValue);
            break;
        case INT64:
            *((int64_t *) retval) = NUM2LL(rbReturnValue);
            break;
        case UINT64:
            *((u_int64_t *) retval) = NUM2ULL(rbReturnValue);
            break;
        case FLOAT32:
            *((float *) retval) = (float) NUM2DBL(rbReturnValue);
            break;
        case FLOAT64:
            *((double *) retval) = NUM2DBL(rbReturnValue);
            break;
        case POINTER:
            *((caddr_t *) retval) = ((AbstractMemory *) DATA_PTR(rbReturnValue))->address;
            break;
        default:
            break;
    }
}

VALUE
rb_FFI_NativeCallback_new(VALUE rbCallbackInfo, VALUE rbProc)
{
    NativeCallback* closure = NULL;
    CallbackInfo* cbInfo = (CallbackInfo *) DATA_PTR(rbCallbackInfo);
    ffi_status status;
    
    closure = ALLOC(NativeCallback);
    closure->ffi_closure = ffi_closure_alloc(sizeof(*closure->ffi_closure), &closure->code);
    if (closure->ffi_closure == NULL) {
        xfree(closure);
        rb_raise(rb_eNoMemError, "Failed to allocate FFI native closure");
    }
    closure->cbInfo = cbInfo;
    closure->rbProc = rbProc;
    closure->rbCallbackInfo = rbCallbackInfo;
    status = ffi_prep_closure_loc(closure->ffi_closure, &cbInfo->ffi_cif,
            native_callback_invoke, closure, closure->code);
    if (status != FFI_OK) {
        ffi_closure_free(closure->ffi_closure);
        xfree(closure);
        rb_raise(rb_eArgError, "ffi_prep_closure_loc failed");
    }
    return Data_Wrap_Struct(classNativeCallback, native_callback_mark, native_callback_free, closure);
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
rb_FFI_Callback_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    rb_FFI_Callback_class = classCallback = rb_define_class_under(moduleFFI, "Callback", rb_cObject);
    rb_define_singleton_method(classCallback, "new", callback_new, 2);
    classNativeCallback = rb_define_class_under(moduleFFI, "NativeCallback", rb_cObject);
    callID = rb_intern("call");
}
