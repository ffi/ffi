#include <ruby.h>
#include "Pointer.h"
#include "rbffi.h"
#include "Callback.h"
#include "Function.h"
#include "Types.h"

static ID id_find = 0;


VALUE
rbffi_NativeValue_ToRuby(Type* type, VALUE rbType, const void* ptr, VALUE enums)
{
    switch (type->nativeType) {
        case NATIVE_VOID:
            return Qnil;
        case NATIVE_INT8:
          return INT2NUM((signed char) *(ffi_sarg *) ptr);
        case NATIVE_INT16:
          return INT2NUM((signed short) *(ffi_sarg *) ptr);
        case NATIVE_INT32:
          return INT2NUM((signed int) *(ffi_sarg *) ptr);
        case NATIVE_UINT8:
          return UINT2NUM((unsigned char) *(ffi_arg *) ptr);
        case NATIVE_UINT16:
          return UINT2NUM((unsigned short) *(ffi_arg *) ptr);
        case NATIVE_UINT32:
          return UINT2NUM((unsigned int) *(ffi_arg *) ptr);
        case NATIVE_INT64:
            return LL2NUM(*(signed long long *) ptr);
        case NATIVE_UINT64:
            return ULL2NUM(*(unsigned long long *) ptr);
        case NATIVE_FLOAT32:
            return rb_float_new(*(float *) ptr);
        case NATIVE_FLOAT64:
            return rb_float_new(*(double *) ptr);
        case NATIVE_STRING:
            return (*(char **)ptr) ? rb_tainted_str_new2(*(char **) ptr) : Qnil;
        case NATIVE_POINTER:
            return rbffi_Pointer_NewInstance(*(void **) ptr);
        case NATIVE_BOOL:
            return ((int) *(ffi_arg *) ptr) ? Qtrue : Qfalse;
        case NATIVE_ENUM:
            return rb_funcall(rbType, id_find, 1, INT2NUM((unsigned int) *(ffi_arg *) ptr));
        
        case NATIVE_FUNCTION:
        case NATIVE_CALLBACK: {
            return rbffi_Function_NewInstance(rbType, rbffi_Pointer_NewInstance(*(void **) ptr));
        }

        default:
            rb_raise(rb_eRuntimeError, "Unknown type: %d", type->nativeType);
            return Qnil;
    }
}

void
rbffi_Types_Init(VALUE moduleFFI)
{
    id_find = rb_intern("find");
}

