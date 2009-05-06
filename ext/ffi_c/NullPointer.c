#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"
#include "Pointer.h"

static VALUE NullPointerErrorClass;
static MemoryOps NullPointerOps;

VALUE rbffi_NullPointerClass;
VALUE rbffi_NullPointerSingleton;

static VALUE
nullptr_allocate(VALUE klass)
{
    AbstractMemory* p;
    VALUE retval;

    retval = Data_Make_Struct(klass, AbstractMemory, NULL, -1, p);
    p->address = 0;
    p->size = 0;
    p->ops = &NullPointerOps;

    return retval;
}

static VALUE
nullptr_op(int argc, VALUE* argv, VALUE self)
{
    rb_raise(NullPointerErrorClass, "NULL Pointer access attempted");
    return Qnil;
}

static VALUE
nullptr_op_get(AbstractMemory* ptr, long offset)
{
    rb_raise(NullPointerErrorClass, "NULL Pointer access attempted");
    return Qnil;
}

static void
nullptr_op_put(AbstractMemory* ptr, long offset, VALUE value)
{
    rb_raise(NullPointerErrorClass, "NULL Pointer access attempted");
}

static VALUE
nullptr_inspect(VALUE self)
{
    return rb_str_new2("#<NULL Pointer address=0x0>");
}

static VALUE
nullptr_null_p(VALUE self)
{
    return Qtrue;
}

static VALUE
nullptr_equals(VALUE self, VALUE other)
{
    AbstractMemory* p2;

    if (!rb_obj_is_kind_of(other, rbffi_PointerClass)) {
        rb_raise(rb_eArgError, "Comparing Pointer with non Pointer");
    }

    Data_Get_Struct(other, AbstractMemory, p2);

    return p2->address == 0 ? Qtrue : Qfalse;
}

static VALUE
nullptr_address(VALUE self)
{
    return INT2NUM(0);
}

static MemoryOp NullPointerMemoryOp = { nullptr_op_get, nullptr_op_put };

static MemoryOps NullPointerOps = {
    .int8 = &NullPointerMemoryOp,
    .uint8 = &NullPointerMemoryOp,
    .int16 = &NullPointerMemoryOp,
    .int16 = &NullPointerMemoryOp,
    .int32 = &NullPointerMemoryOp,
    .uint32 = &NullPointerMemoryOp,
    .int64 = &NullPointerMemoryOp,
    .uint64 = &NullPointerMemoryOp,
    .float32 = &NullPointerMemoryOp,
    .float64 = &NullPointerMemoryOp,
    .pointer = &NullPointerMemoryOp,
    .strptr = &NullPointerMemoryOp,
};

void
rbffi_NullPointer_Init(VALUE moduleFFI)
{
    rbffi_NullPointerClass = rb_define_class_under(moduleFFI, "NullPointer", rbffi_PointerClass);
    rb_global_variable(&rbffi_NullPointerClass);
    
    NullPointerErrorClass = rb_define_class_under(moduleFFI, "NullPointerError", rb_eRuntimeError);
    rb_global_variable(&NullPointerErrorClass);

    rb_define_alloc_func(rbffi_NullPointerClass, nullptr_allocate);
    rb_define_method(rbffi_NullPointerClass, "inspect", nullptr_inspect, 0);
    rb_define_method(rbffi_NullPointerClass, "+", nullptr_op, -1);
    rb_define_method(rbffi_NullPointerClass, "null?", nullptr_null_p, 0);
    rb_define_method(rbffi_NullPointerClass, "address", nullptr_address, 0);
    rb_define_method(rbffi_NullPointerClass, "==", nullptr_equals, 1);

#define NOP(name) \
    rb_define_method(rbffi_NullPointerClass, "get_" #name, nullptr_op, -1); \
    rb_define_method(rbffi_NullPointerClass, "put_" #name, nullptr_op, -1); \
    rb_define_method(rbffi_NullPointerClass, "get_array_of_" #name, nullptr_op, -1); \
    rb_define_method(rbffi_NullPointerClass, "put_array_of_" #name, nullptr_op, -1); \


    NOP(int8); NOP(uint8); NOP(int16); NOP(uint16);
    NOP(int32); NOP(uint32); NOP(int64); NOP(uint64); NOP(long); NOP(ulong);
    NOP(float32); NOP(float64);
    NOP(char); NOP(uchar); NOP(short); NOP(ushort);
    NOP(int); NOP(uint); NOP(long_long); NOP(ulong_long);
    NOP(float); NOP(double);

#undef NOP
#define NOP(name) \
    rb_define_method(rbffi_NullPointerClass, "get_" #name, nullptr_op,  -1); \
    rb_define_method(rbffi_NullPointerClass, "put_" #name, nullptr_op, -1);

    NOP(string); NOP(bytes); NOP(pointer); NOP(callback);

    rb_define_method(rbffi_NullPointerClass, "clear", nullptr_op, -1); \
    rb_define_method(rbffi_NullPointerClass, "total", nullptr_op, -1);

    // Create a singleton instance of NullPointer that can be shared
    rbffi_NullPointerSingleton = nullptr_allocate(rbffi_NullPointerClass);
    rb_global_variable(&rbffi_NullPointerSingleton);
}

