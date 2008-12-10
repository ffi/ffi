#include <sys/types.h>
#include <stdio.h>
#include <dlfcn.h>
#include <ruby.h>

#include <ffi.h>

#include "rbffi.h"
#include "AbstractMemory.h"
#include "Pointer.h"
#include "MemoryPointer.h"
#include "AutoPointer.h"
#include "Struct.h"
#include "NativeLibrary.h"
#include "Platform.h"
#include "Types.h"


void Init_ffi();

static VALUE moduleFFI = Qnil;
static VALUE moduleNativeType = Qnil;

void
Init_ffi_c() {
    moduleFFI = rb_define_module("FFI");
    moduleNativeType = rb_define_module_under(moduleFFI, "NativeType");
    
    rb_define_const(moduleNativeType, "VOID", INT2FIX(NATIVE_VOID));
    rb_define_const(moduleNativeType, "INT8", INT2FIX(NATIVE_INT8));
    rb_define_const(moduleNativeType, "UINT8", INT2FIX(NATIVE_UINT8));
    rb_define_const(moduleNativeType, "INT16", INT2FIX(NATIVE_INT16));
    rb_define_const(moduleNativeType, "UINT16", INT2FIX(NATIVE_UINT16));
    rb_define_const(moduleNativeType, "INT32", INT2FIX(NATIVE_INT32));
    rb_define_const(moduleNativeType, "UINT32", INT2FIX(NATIVE_UINT32));
    rb_define_const(moduleNativeType, "INT64", INT2FIX(NATIVE_INT64));
    rb_define_const(moduleNativeType, "UINT64", INT2FIX(NATIVE_UINT64));
    rb_define_const(moduleNativeType, "FLOAT32", INT2FIX(NATIVE_FLOAT32));
    rb_define_const(moduleNativeType, "FLOAT64", INT2FIX(NATIVE_FLOAT64));
    rb_define_const(moduleNativeType, "POINTER", INT2FIX(NATIVE_POINTER));
    rb_define_const(moduleNativeType, "STRING", INT2FIX(NATIVE_STRING));
    rb_define_const(moduleNativeType, "RBXSTRING", INT2FIX(NATIVE_RBXSTRING));
    rb_define_const(moduleNativeType, "CHAR_ARRAY", INT2FIX(NATIVE_CHAR_ARRAY));
    rb_define_const(moduleNativeType, "BUFFER_IN", INT2FIX(NATIVE_BUFFER_IN));
    rb_define_const(moduleNativeType, "BUFFER_OUT", INT2FIX(NATIVE_BUFFER_OUT));
    rb_define_const(moduleNativeType, "BUFFER_INOUT", INT2FIX(NATIVE_BUFFER_INOUT));
    rb_define_const(moduleNativeType, "VARARGS", INT2FIX(NATIVE_VARARGS));
    if (sizeof(long) == 4) {
        rb_define_const(moduleNativeType, "LONG", INT2FIX(NATIVE_INT32));
        rb_define_const(moduleNativeType, "ULONG", INT2FIX(NATIVE_UINT32));
    } else {
        rb_define_const(moduleNativeType, "LONG", INT2FIX(NATIVE_INT64));
        rb_define_const(moduleNativeType, "ULONG", INT2FIX(NATIVE_UINT64));
    }
    rb_FFI_Platform_Init();
    rb_FFI_AbstractMemory_Init();
    rb_FFI_Pointer_Init();
    rb_FFI_AutoPointer_Init();
    rb_FFI_NullPointer_Init();
    rb_FFI_MemoryPointer_Init();
    rb_FFI_Buffer_Init();
    rb_FFI_Callback_Init();
    rb_FFI_Struct_Init(0);
    rb_FFI_NativeLibrary_Init();
    rb_FFI_Invoker_Init();
}

