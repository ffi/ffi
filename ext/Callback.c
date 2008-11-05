#include <sys/types.h>
#include <ruby.h>
#include "MemoryPointer.h"
#include "AbstractMemory.h"
#include "Callback.h"
#include "Types.h"
#include "rbffi.h"



static void callback_mark(CallbackInfo *);
static void callback_free(CallbackInfo *);

static VALUE classCallback = Qnil;
static VALUE classNativeCallback = Qnil;
static ID callID = Qnil;

//static VALUE classCallbackImpl = Qnil;
VALUE rb_FFI_Callback_class = Qnil;

static VALUE
callback_new(VALUE self, VALUE rbReturnType, VALUE rbParamTypes)
{
    CallbackInfo *cbInfo;
    VALUE retval;
    int paramCount = RARRAY_LEN(rbParamTypes);
    ffi_status status;
    int i;

    retval = Data_Make_Struct(classCallback, CallbackInfo, callback_mark, callback_free, cbInfo);
    cbInfo->parameterCount = paramCount;
    cbInfo->parameterTypes = calloc(paramCount, sizeof(NativeType));
    cbInfo->ffiParameterTypes = calloc(paramCount, sizeof(ffi_type *));
    if (cbInfo->parameterTypes == NULL || cbInfo->ffiParameterTypes == NULL) {
        callback_free(cbInfo);
        rb_raise(rb_eNoMemError, "Failed to allocate native memory");
    }
    for (i = 0; i < paramCount; ++i) {
        cbInfo->parameterTypes[i] = FIX2INT(rb_ary_entry(rbParamTypes, i));
        cbInfo->ffiParameterTypes[i] = rb_FFI_NativeTypeToFFI(cbInfo->parameterTypes[i]);
        if (cbInfo->ffiParameterTypes[i] == NULL) {
            callback_free(cbInfo);
            rb_raise(rb_eArgError, "Unknown argument type: %#x", cbInfo->parameterTypes[i]);
        }
    }
    cbInfo->returnType = FIX2INT(rbReturnType);
    cbInfo->ffiReturnType = rb_FFI_NativeTypeToFFI(cbInfo->returnType);
    if (cbInfo->ffiReturnType == NULL) {
        callback_free(cbInfo);
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
            callback_free(cbInfo);
            rb_raise(rb_eArgError, "Invalid ABI specified");
        case FFI_BAD_TYPEDEF:
            callback_free(cbInfo);
            rb_raise(rb_eArgError, "Invalid argument type specified");
        case FFI_OK:
            break;
        default:
            callback_free(cbInfo);
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
            free(cbInfo->parameterTypes);
        }
        if (cbInfo->ffiParameterTypes != NULL) {
            free(cbInfo->ffiParameterTypes);
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
                param = INT2FIX(*(int8_t *) parameters[i]);
                break;
            case UINT8:
                param = UINT2NUM(*(u_int8_t *) parameters[i]);
                break;
            case INT16:
                param = INT2FIX(*(int16_t *) parameters[i]);
                break;
            case UINT16:
                param = UINT2NUM(*(u_int16_t *) parameters[i]);
                break;
            case INT32:
                param = INT2FIX(*(int32_t *) parameters[i]);
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
                param = rb_str_new2(*(char **) parameters[i]);
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
            *((int8_t *) retval) = NUM2INT(rbReturnValue);
            break;
        case UINT8:
            *((u_int8_t *) retval) = NUM2UINT(rbReturnValue);
            break;
        case INT16:
            *((int16_t *) retval) = NUM2INT(rbReturnValue);
            break;
        case UINT16:
            *((u_int16_t *) retval) = NUM2UINT(rbReturnValue);
            break;
        case INT32:
            *((int32_t *) retval) = NUM2INT(rbReturnValue);
            break;
        case UINT32:
            *((u_int32_t *) retval) = NUM2UINT(rbReturnValue);
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

void
rb_FFI_Callback_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    rb_FFI_Callback_class = classCallback = rb_define_class_under(moduleFFI, "Callback", rb_cObject);
    rb_define_singleton_method(classCallback, "new", callback_new, 2);
    classNativeCallback = rb_define_class_under(moduleFFI, "NativeCallback", rb_cObject);
    callID = rb_intern("call");
}
