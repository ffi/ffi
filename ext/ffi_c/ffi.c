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


void Init_ffi_c();

static VALUE moduleFFI = Qnil;
static VALUE typeMap = Qnil, sizeMap = Qnil;
static ID type_size_id = 0, size_id;

int
rb_FFI_type_size(VALUE type)
{
    int t = TYPE(type);
    if (t == T_FIXNUM || t == T_BIGNUM) {
        return NUM2INT(type);
    } else if (t == T_SYMBOL) {
        /*
         * Try looking up directly in the type and size maps
         */
        VALUE nType;
        if ((nType = rb_hash_aref(typeMap, type)) != Qnil) {
            VALUE nSize = rb_hash_aref(sizeMap, nType);
            if (TYPE(nSize) == T_FIXNUM) {
                return FIX2INT(nSize);
            }
        }
        // Not found - call up to the ruby version to resolve
        return NUM2INT(rb_funcall2(moduleFFI, type_size_id, 1, &type));
    } else {
        return NUM2INT(rb_funcall2(type, size_id, 0, NULL));
    }
}

void
Init_ffi_c() {
    moduleFFI = rb_define_module("FFI");
    rb_global_variable(&moduleFFI);
    rb_define_const(moduleFFI, "TypeDefs", typeMap = rb_hash_new());
    rb_define_const(moduleFFI, "SizeTypes", sizeMap = rb_hash_new());
    rb_global_variable(&typeMap);
    rb_global_variable(&sizeMap);
    type_size_id = rb_intern("type_size");
    size_id = rb_intern("size");

    rb_FFI_Type_Init(moduleFFI);

    rb_FFI_Platform_Init(moduleFFI);
    rb_FFI_AbstractMemory_Init(moduleFFI);
    rb_FFI_Pointer_Init(moduleFFI);
    rb_FFI_AutoPointer_Init(moduleFFI);
    rb_FFI_NullPointer_Init(moduleFFI);
    rb_FFI_MemoryPointer_Init(moduleFFI);
    rb_FFI_Buffer_Init(moduleFFI);
    rb_FFI_Callback_Init(moduleFFI);
    rb_FFI_Struct_Init(moduleFFI);
    rb_FFI_DynamicLibrary_Init(moduleFFI);
    rb_FFI_Invoker_Init(moduleFFI);
    rb_FFI_Types_Init(moduleFFI);
}

