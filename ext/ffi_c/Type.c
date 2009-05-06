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

VALUE rbffi_TypeClass = Qnil;

static VALUE classBuiltinType = Qnil;

static VALUE
type_allocate(VALUE klass)
{
    Type* type;
    VALUE obj = Data_Make_Struct(klass, Type, NULL, -1, type);

    type->nativeType = -1;

    return obj;
}

static VALUE
type_initialize(VALUE self, VALUE value)
{
    Type* type;
    Type* other;

    Data_Get_Struct(self, Type, type);

    if (FIXNUM_P(value)) {
        type->nativeType = FIX2INT(value);
    } else if (rb_obj_is_kind_of(value, rbffi_TypeClass)) {
        Data_Get_Struct(value, Type, other);
        type->nativeType = other->nativeType;
    } else {
        rb_raise(rb_eArgError, "wrong type");
    }
    
    return self;
}

static VALUE
enum_allocate(VALUE klass)
{
    Type* type;
    VALUE obj;

    obj = Data_Make_Struct(klass, Type, NULL, -1, type);
    type->nativeType = NATIVE_ENUM;

    return obj;
}

int
rbffi_Type_GetIntValue(VALUE type)
{
    if (rb_obj_is_kind_of(type, rbffi_TypeClass)) {
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
rbffi_Type_Init(VALUE moduleFFI)
{
    VALUE moduleNativeType;
    VALUE classType = rbffi_TypeClass = rb_define_class_under(moduleFFI, "Type", rb_cObject);
    VALUE classEnum =  rb_define_class_under(moduleFFI, "Enum", classType);
    classBuiltinType = rb_define_class_under(rbffi_TypeClass, "Builtin", rbffi_TypeClass);
    moduleNativeType = rb_define_module_under(moduleFFI, "NativeType");

    rb_global_variable(&rbffi_TypeClass);
    rb_global_variable(&classBuiltinType);
    rb_global_variable(&moduleNativeType);

    rb_define_alloc_func(classType, type_allocate);
    rb_define_method(classType, "initialize", type_initialize, 1);

    // Make Type::Builtin non-allocatable
    rb_undef_method(CLASS_OF(classBuiltinType), "new");
    rb_define_method(classBuiltinType, "inspect", builtin_type_inspect, 0);
    rb_define_method(classBuiltinType, "size", builtin_type_size, 0);

    rb_define_alloc_func(classEnum, enum_allocate);

    rb_global_variable(&rbffi_TypeClass);
    rb_global_variable(&classBuiltinType);

    // Define all the builtin types
    #define T(x, size) do { \
        VALUE t = Qnil; \
        rb_define_const(classType, #x, t = builtin_type_new(classBuiltinType, NATIVE_##x, #x, size)); \
        rb_define_const(moduleNativeType, #x, t); \
        rb_define_const(moduleFFI, "TYPE_" #x, t); \
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
