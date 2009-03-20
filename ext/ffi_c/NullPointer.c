#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"
#include "Pointer.h"

static VALUE NullPointerError;
static VALUE classNullPointer;
static MemoryOps nullptr_ops;

VALUE rb_FFI_NullPointer_class;
VALUE rb_FFI_NullPointer_singleton;

VALUE
rb_FFI_NullPointer_allocate(VALUE klass)
{
    AbstractMemory* p;
    VALUE retval;

    retval = Data_Make_Struct(klass, AbstractMemory, NULL, -1, p);
    p->address = 0;
    p->size = 0;
    p->ops = &nullptr_ops;

    return retval;
}

static VALUE
nullptr_op(int argc, VALUE* argv, VALUE self)
{
    rb_raise(NullPointerError, "NULL Pointer access attempted");
}

static VALUE
nullptr_op_get(AbstractMemory* ptr, long offset)
{
    rb_raise(NullPointerError, "NULL Pointer access attempted");
}

static void
nullptr_op_put(AbstractMemory* ptr, long offset, VALUE value)
{
    rb_raise(NullPointerError, "NULL Pointer access attempted");
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

    if (!rb_obj_is_kind_of(other, rb_FFI_Pointer_class)) {
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

static MemoryOp nullptr_memory_op = { nullptr_op_get, nullptr_op_put };

static MemoryOps nullptr_ops = {
    .int8 = &nullptr_memory_op,
    .uint8 = &nullptr_memory_op,
    .int16 = &nullptr_memory_op,
    .int16 = &nullptr_memory_op,
    .int32 = &nullptr_memory_op,
    .uint32 = &nullptr_memory_op,
    .int64 = &nullptr_memory_op,
    .uint64 = &nullptr_memory_op,
    .float32 = &nullptr_memory_op,
    .float64 = &nullptr_memory_op,
    .pointer = &nullptr_memory_op,
    .strptr = &nullptr_memory_op,
};

void
rb_FFI_NullPointer_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    rb_FFI_NullPointer_class = classNullPointer = rb_define_class_under(moduleFFI, "NullPointer", rb_FFI_Pointer_class);
    NullPointerError = rb_define_class_under(moduleFFI, "NullPointerError", rb_eRuntimeError);
    rb_define_alloc_func(classNullPointer, rb_FFI_NullPointer_allocate);
    rb_define_method(classNullPointer, "inspect", nullptr_inspect, 0);
    rb_define_method(classNullPointer, "+", nullptr_op, -1);
    rb_define_method(classNullPointer, "null?", nullptr_null_p, 0);
    rb_define_method(classNullPointer, "address", nullptr_address, 0);
    rb_define_method(classNullPointer, "==", nullptr_equals, 1);

#define NOP(name) \
    rb_define_method(classNullPointer, "get_" #name, nullptr_op, -1); \
    rb_define_method(classNullPointer, "put_" #name, nullptr_op, -1); \
    rb_define_method(classNullPointer, "get_array_of_" #name, nullptr_op, -1); \
    rb_define_method(classNullPointer, "put_array_of_" #name, nullptr_op, -1); \


    NOP(int8); NOP(uint8); NOP(int16); NOP(uint16);
    NOP(int32); NOP(uint32); NOP(int64); NOP(uint64); NOP(long); NOP(ulong);
    NOP(float32); NOP(float64);
    NOP(char); NOP(uchar); NOP(short); NOP(ushort);
    NOP(int); NOP(uint); NOP(long_long); NOP(ulong_long);
    NOP(float); NOP(double);

#undef NOP
#define NOP(name) \
    rb_define_method(classNullPointer, "get_" #name, nullptr_op,  -1); \
    rb_define_method(classNullPointer, "put_" #name, nullptr_op, -1);

    NOP(string); NOP(bytes); NOP(pointer); NOP(callback);

    rb_define_method(classNullPointer, "clear", nullptr_op, -1); \
    rb_define_method(classNullPointer, "total", nullptr_op, -1);

    // Create a singleton instance of NullPointer that can be shared
    rb_FFI_NullPointer_singleton = rb_FFI_NullPointer_allocate(classNullPointer);
    rb_gc_register_address(&rb_FFI_NullPointer_singleton);
}

