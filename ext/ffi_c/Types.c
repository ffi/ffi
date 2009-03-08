#include <ruby.h>
#include "Pointer.h"
#include "Types.h"

ffi_type*
rb_FFI_NativeTypeToFFI(NativeType type)
{
    switch (type) {
        case NATIVE_VOID:
            return &ffi_type_void;
        case NATIVE_INT8:
            return &ffi_type_sint8;
        case NATIVE_UINT8:
            return &ffi_type_uint8;
        case NATIVE_INT16:
            return &ffi_type_sint16;
        case NATIVE_UINT16:
            return &ffi_type_uint16;
        case NATIVE_INT32:
            return &ffi_type_sint32;
        case NATIVE_UINT32:
            return &ffi_type_uint32;
        case NATIVE_INT64:
            return &ffi_type_sint64;
        case NATIVE_UINT64:
            return &ffi_type_uint64;
        case NATIVE_FLOAT32:
            return &ffi_type_float;
        case NATIVE_FLOAT64:
            return &ffi_type_double;
        case NATIVE_STRING:
        case NATIVE_RBXSTRING:
        case NATIVE_POINTER:
        case NATIVE_BUFFER_IN:
        case NATIVE_BUFFER_OUT:
        case NATIVE_BUFFER_INOUT:
            return &ffi_type_pointer;
        default:
            return NULL;
    }
}

VALUE
rb_FFI_NativeValueToRuby(NativeType type, const void* ptr)
{
    switch (type) {
        case NATIVE_VOID:
            return Qnil;
        case NATIVE_INT8:
        case NATIVE_INT16:
        case NATIVE_INT32:
          return INT2NUM((*(long *) ptr) & 0xffffffffL);
        case NATIVE_UINT8:
        case NATIVE_UINT16:
        case NATIVE_UINT32:
          return UINT2NUM((*(unsigned long *) ptr) & 0xffffffffL);
        case NATIVE_INT64:
            return LL2NUM(*(signed long long *) ptr);
        case NATIVE_UINT64:
            return ULL2NUM(*(unsigned long long *) ptr);
        case NATIVE_FLOAT32:
            return rb_float_new(*(float *) ptr);
        case NATIVE_FLOAT64:
            return rb_float_new(*(double *) ptr);
        case NATIVE_STRING:
            return rb_tainted_str_new2(*(char **) ptr);
        case NATIVE_POINTER:
            return rb_FFI_Pointer_new(*(void **) ptr);
        default:
            rb_raise(rb_eRuntimeError, "Unknown type: %d", type);
    }
}
