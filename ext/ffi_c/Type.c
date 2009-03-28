#include <sys/param.h>
#include <sys/types.h>
#include <ruby.h>
#include <ffi.h>
#include "rbffi.h"
#include "compat.h"
#include "Types.h"
#include "Type.h"


typedef struct BuiltinType_ {
    Type type;
    char* name;
    int size;
} BuiltinType;

static void builtin_type_free(BuiltinType *);

VALUE  rb_FFI_Type_class = Qnil;

static VALUE classBuiltinType = Qnil;

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

int
rb_FFI_Type_GetIntValue(VALUE type)
{
    if (rb_obj_is_kind_of(type, rb_FFI_Type_class)) {
        Type* t;
        Data_Get_Struct(type, Type, t);
        return t->nativeType;
    } else {
        rb_raise(rb_eArgError, "Invalid type argument");
    }
}

static VALUE
builtin_type_new(VALUE klass, int nativeType, const char* name, int size)
{
    BuiltinType* type;
    VALUE obj = Data_Make_Struct(klass, BuiltinType, NULL, builtin_type_free, type);
    type->type.nativeType = nativeType;
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
    VALUE classType = rb_FFI_Type_class = rb_define_class_under(moduleFFI, "Type", rb_cObject);
    classBuiltinType = rb_define_class_under(rb_FFI_Type_class, "Builtin", rb_FFI_Type_class);
    moduleNativeType = rb_define_module_under(moduleFFI, "NativeType");

    rb_define_alloc_func(classType, type_allocate);
    rb_define_method(classType, "initialize", type_initialize, 0);

    // Make Type::Builtin non-allocatable
    rb_undef_method(CLASS_OF(classBuiltinType), "new");
    rb_define_method(classBuiltinType, "inspect", builtin_type_inspect, 0);
    rb_define_method(classBuiltinType, "size", builtin_type_size, 0);

    rb_global_variable(&rb_FFI_Type_class);
    rb_global_variable(&classBuiltinType);

    // Define all the builtin types
    #define T(x, size) do { \
        VALUE t = Qnil; \
        rb_define_const(classType, #x, t = builtin_type_new(classBuiltinType, NATIVE_##x, #x, size)); \
        rb_define_const(moduleNativeType, #x, t); \
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
        VALUE t = Qnil;
        rb_define_const(classType, "LONG", t = builtin_type_new(classBuiltinType, NATIVE_INT32, "LONG", 4));
        rb_define_const(moduleNativeType, "LONG", t);
        rb_define_const(classType, "ULONG", t = builtin_type_new(classBuiltinType, NATIVE_UINT32, "ULONG", 4));
        rb_define_const(moduleNativeType, "ULONG", t);
    } else {
        VALUE t = Qnil;
        rb_define_const(classType, "LONG", t = builtin_type_new(classBuiltinType, NATIVE_INT64, "LONG", 4));
        rb_define_const(moduleNativeType, "LONG", t);
        rb_define_const(classType, "ULONG", t = builtin_type_new(classBuiltinType, NATIVE_UINT64, "ULONG", 4));
        rb_define_const(moduleNativeType, "ULONG", t);
    }
}
