#include <sys/types.h>
#include <stdio.h>
#include <ruby.h>

#include <ffi.h>

#include "rbffi.h"
#include "AbstractMemory.h"
#include "Pointer.h"
#include "MemoryPointer.h"
#include "AutoPointer.h"
#include "Struct.h"
#include "Callback.h"
#include "DynamicLibrary.h"
#include "Platform.h"
#include "Types.h"
#include "LastError.h"
#include "Function.h"

void Init_ffi_c(void);

VALUE rbffi_FFIModule = Qnil;

static VALUE moduleFFI = Qnil;

void
Init_ffi_c(void) {
    rbffi_FFIModule = moduleFFI = rb_define_module("FFI");
    rb_global_variable(&moduleFFI);
    
    rbffi_Type_Init(moduleFFI);
    rbffi_LastError_Init(moduleFFI);
    rbffi_Platform_Init(moduleFFI);
    rbffi_AbstractMemory_Init(moduleFFI);
    rbffi_Pointer_Init(moduleFFI);
    rbffi_AutoPointer_Init(moduleFFI);
    rbffi_NullPointer_Init(moduleFFI);
    rbffi_Function_Init(moduleFFI);
    rbffi_MemoryPointer_Init(moduleFFI);
    rbffi_Buffer_Init(moduleFFI);
    rbffi_Callback_Init(moduleFFI);
    rbffi_Struct_Init(moduleFFI);
    rbffi_DynamicLibrary_Init(moduleFFI);
    rbffi_Invoker_Init(moduleFFI);
    rbffi_Types_Init(moduleFFI);
}

