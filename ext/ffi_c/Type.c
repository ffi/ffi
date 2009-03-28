#include <sys/param.h>
#include <sys/types.h>
#include <ruby.h>
#include <ffi.h>
#include "rbffi.h"
#include "compat.h"
#include "Types.h"

typedef struct Type_ {
    NativeType nativeType;
} Type;

typedef struct BuiltinType_ {
    NativeType nativeType;
    char* name;
    int size;
} BuiltinType;

static void builtin_type_free(BuiltinType *);

static VALUE classType = Qnil, classBuiltinType = Qnil;

static VALUE
type_allocate(VALUE klass)
{
    Type* type;
    return Data_Make_Struct(klass, Type, NULL, -1, type);
}

static VALUE
type_initialize(VALUE self)
{
    return self;
}

static VALUE
builtin_type_new(VALUE klass, int nativeType, const char* name, int size)
{
    BuiltinType* type;
    VALUE obj = Data_Make_Struct(klass, BuiltinType, NULL, builtin_type_free, type);
    type->nativeType = nativeType;
    type->name = strdup(name);
    type->size = size;

    return obj;
}

static void
builtin_type_free(BuiltinType *type)
{
    free(type->name);
    xfree(type);
}

static VALUE
builtin_type_inspect(VALUE self)
{
    char buf[256];
    BuiltinType *type;
    Data_Get_Struct(self, BuiltinType, type);
    snprintf(buf, sizeof(buf), "#<FFI::Type::Builtin:%s>", type->name);

    return rb_str_new2(buf);
}

static VALUE
builtin_type_size(VALUE self)
{
    BuiltinType *type;
    Data_Get_Struct(self, BuiltinType, type);

    return INT2FIX(type->size);
}

void
rb_FFI_Type_Init(VALUE moduleFFI)
{
    VALUE moduleNativeType;
    classType = rb_define_class_under(moduleFFI, "Type", rb_cObject);
    classBuiltinType = rb_define_class_under(classType, "Builtin", classType);
    moduleNativeType = rb_define_module_under(moduleFFI, "NativeType");

    rb_define_alloc_func(classType, type_allocate);
    rb_define_method(classType, "initialize", type_initialize, 0);

    // Make Type::Builtin non-allocatable
    rb_undef_method(CLASS_OF(classBuiltinType), "new");
    rb_define_method(classBuiltinType, "inspect", builtin_type_inspect, 0);
    rb_define_method(classBuiltinType, "size", builtin_type_size, 0);

    rb_global_variable(&classType);
    rb_global_variable(&classBuiltinType);

    // Define all the builtin types
    #define T(x, size) do { \
        rb_define_const(classType, #x, builtin_type_new(classBuiltinType, NATIVE_##x, #x, size)); \
        rb_define_const(moduleNativeType, #x, INT2FIX(NATIVE_##x)); \
    } while(0)

    T(VOID, 0);
    T(INT8, 2);
    T(UINT8, 2);
    T(INT16, 2);
    T(UINT16, 2);
    T(INT32, 4);
    T(UINT32, 4);
    T(INT64, 8);
    T(UINT64, 8);
    T(FLOAT32, 4);
    T(FLOAT64, 8);
    T(POINTER, sizeof(void *));
    T(STRING, sizeof(void *));
    T(RBXSTRING, sizeof(void *));
    T(CHAR_ARRAY, 0);
    T(BUFFER_IN, sizeof(void *));
    T(BUFFER_OUT, sizeof(void *));
    T(BUFFER_INOUT, sizeof(void *));
    T(VARARGS, 0);
    T(ENUM, sizeof(int));

    if (sizeof(long) == 4) {
        rb_define_const(classType, "LONG", builtin_type_new(classBuiltinType, NATIVE_INT32, "LONG", 4));
        rb_define_const(classType, "ULONG", builtin_type_new(classBuiltinType, NATIVE_UINT32, "ULONG", 4));
        rb_define_const(moduleNativeType, "LONG", INT2FIX(NATIVE_INT32));
        rb_define_const(moduleNativeType, "ULONG", INT2FIX(NATIVE_UINT32));
    } else {
        rb_define_const(classType, "LONG", builtin_type_new(classBuiltinType, NATIVE_INT64, "LONG", 8));
        rb_define_const(classType, "ULONG", builtin_type_new(classBuiltinType, NATIVE_UINT64, "ULONG", 8));
        rb_define_const(moduleNativeType, "LONG", INT2FIX(NATIVE_INT64));
        rb_define_const(moduleNativeType, "ULONG", INT2FIX(NATIVE_UINT64));
    }
}
