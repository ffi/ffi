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
    
    rb_define_const(moduleNativeType, "VOID", INT2FIX(VOID));
    rb_define_const(moduleNativeType, "INT8", INT2FIX(INT8));
    rb_define_const(moduleNativeType, "UINT8", INT2FIX(UINT8));
    rb_define_const(moduleNativeType, "INT16", INT2FIX(INT16));
    rb_define_const(moduleNativeType, "UINT16", INT2FIX(UINT16));
    rb_define_const(moduleNativeType, "INT32", INT2FIX(INT32));
    rb_define_const(moduleNativeType, "UINT32", INT2FIX(UINT32));
    rb_define_const(moduleNativeType, "INT64", INT2FIX(INT64));
    rb_define_const(moduleNativeType, "UINT64", INT2FIX(UINT64));
    rb_define_const(moduleNativeType, "FLOAT32", INT2FIX(FLOAT32));
    rb_define_const(moduleNativeType, "FLOAT64", INT2FIX(FLOAT64));
    rb_define_const(moduleNativeType, "POINTER", INT2FIX(POINTER));
    rb_define_const(moduleNativeType, "STRING", INT2FIX(STRING));
    rb_define_const(moduleNativeType, "RBXSTRING", INT2FIX(RBXSTRING));
    rb_define_const(moduleNativeType, "CHAR_ARRAY", INT2FIX(CHAR_ARRAY));
    rb_define_const(moduleNativeType, "BUFFER_IN", INT2FIX(BUFFER_IN));
    rb_define_const(moduleNativeType, "BUFFER_OUT", INT2FIX(BUFFER_OUT));
    rb_define_const(moduleNativeType, "BUFFER_INOUT", INT2FIX(BUFFER_INOUT));
    rb_define_const(moduleNativeType, "VARARGS", INT2FIX(VARARGS));
    if (sizeof(long) == 4) {
        rb_define_const(moduleNativeType, "LONG", INT2FIX(INT32));
        rb_define_const(moduleNativeType, "ULONG", INT2FIX(UINT32));
    } else {
        rb_define_const(moduleNativeType, "LONG", INT2FIX(INT64));
        rb_define_const(moduleNativeType, "ULONG", INT2FIX(UINT64));
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

