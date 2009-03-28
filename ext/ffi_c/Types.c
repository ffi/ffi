#include <ruby.h>
#include "Pointer.h"
#include "rbffi.h"
#include "Callback.h"
#include "Types.h"

static ID find_id = 0;

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
        case NATIVE_ENUM:
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
        case NATIVE_CALLBACK:
            return &ffi_type_pointer;
        default:
            return NULL;
    }
}

VALUE
rb_FFI_NativeValueToRuby(NativeType type, VALUE rbType, const void* ptr, VALUE enums)
{
    switch (type) {
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
            return rb_tainted_str_new2(*(char **) ptr);
        case NATIVE_POINTER:
            return rb_FFI_Pointer_new(*(void **) ptr);
        case NATIVE_ENUM:
        {
            VALUE enum_obj = rb_funcall(enums, find_id, 1, rbType);
            if (enum_obj == Qnil) {
                VALUE s = rb_inspect(rbType);
                rb_raise(rb_eRuntimeError, "Unknown enumeration: %s", StringValueCStr(s));
            }
            return rb_funcall(enum_obj, find_id, 1, INT2NUM((unsigned int) *(ffi_arg *) ptr));
        }
        case NATIVE_CALLBACK: {
            CallbackInfo* cbInfo;
            VALUE argv[6];
            VALUE funcptr = rb_FFI_Pointer_new(*(void **) ptr);

            Data_Get_Struct(rbType, CallbackInfo, cbInfo);
            argv[0] = funcptr;
            argv[1] = cbInfo->rbParameterTypes;
            argv[2] = ID2SYM(rb_intern("cb")); // just shove a dummy value
            argv[3] = cbInfo->rbReturnType;
            argv[4] = rb_str_new2("default");
            argv[5] = Qnil;

            return rb_class_new_instance(6, argv, rb_FFI_Invoker_class);
        }

        default:
            rb_raise(rb_eRuntimeError, "Unknown type: %d", type);
    }
}

void rb_FFI_Types_Init(void)
{
    find_id = rb_intern("find");
}