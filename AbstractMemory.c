#include <sys/types.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"

static VALUE memory_initialize(VALUE self/*, VALUE address, VALUE size*/);
static VALUE memory_put_int64(VALUE self, VALUE offset, VALUE value);
static VALUE memory_get_int64(VALUE self, VALUE offset);
static VALUE memory_put_uint64(VALUE self, VALUE offset, VALUE value);
static VALUE memory_get_uint64(VALUE self, VALUE offset);

static VALUE memory_put_float32(VALUE self, VALUE offset, VALUE value);
static VALUE memory_get_float32(VALUE self, VALUE offset);
static VALUE memory_put_float64(VALUE self, VALUE offset, VALUE value);
static VALUE memory_get_float64(VALUE self, VALUE offset);
static inline caddr_t memory_address(VALUE self);

VALUE rb_FFI_AbstractMemory_class = Qnil;
static VALUE classMemory = Qnil;

static VALUE 
memory_initialize(VALUE self/*, VALUE address, VALUE size*/)
{
    return self;
}

#define ADDRESS(self, offset) (memory_address((self)) + NUM2ULONG(offset))

#define INT_OP(name, type) \
static VALUE memory_put_##name(VALUE self, VALUE offset, VALUE value); \
static VALUE \
memory_put_##name(VALUE self, VALUE offset, VALUE value) \
{ \
    *(type *) ADDRESS(self, offset) = (type) NUM2INT(value); \
    return self; \
} \
static VALUE memory_get_##name(VALUE self, VALUE offset); \
static VALUE \
memory_get_##name(VALUE self, VALUE offset) \
{ \
    return INT2FIX(*(type *) ADDRESS(self, offset)); \
}
#define INT(type) INT_OP(type, type##_t); INT_OP(u##type, u_##type##_t)
INT(int8);
INT(int16);
INT(int32);

static VALUE
memory_put_int64(VALUE self, VALUE offset, VALUE value)
{
    *(int64_t *) ADDRESS(self, offset) = NUM2LL(value);
    return self;
}
static VALUE memory_get_int64(VALUE self, VALUE offset)
{
    return LL2NUM(*(int64_t *) ADDRESS(self, offset));
}

static VALUE
memory_put_uint64(VALUE self, VALUE offset, VALUE value)
{
    *(u_int64_t *) ADDRESS(self, offset) = NUM2ULL(value);
    return self;
}

static VALUE memory_get_uint64(VALUE self, VALUE offset)
{
    return ULL2NUM(*(u_int64_t *) ADDRESS(self, offset));
}

static VALUE
memory_put_float32(VALUE self, VALUE offset, VALUE value)
{
    *(float *) ADDRESS(self, offset) = NUM2DBL(value);
    return self;
}

static VALUE memory_get_float32(VALUE self, VALUE offset)
{
    return rb_float_new(*(float *) ADDRESS(self, offset));
}

static VALUE
memory_put_float64(VALUE self, VALUE offset, VALUE value)
{
    *(double *) ADDRESS(self, offset) = NUM2DBL(value);
    return self;
}

static VALUE memory_get_float64(VALUE self, VALUE offset)
{
    return rb_float_new(*(double *) ADDRESS(self, offset));
}

static inline caddr_t
memory_address(VALUE self)
{
    return ((AbstractMemory *)DATA_PTR((self)))->address;
}

void
rb_FFI_AbstractMemory_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    rb_FFI_AbstractMemory_class = classMemory = rb_define_class_under(moduleFFI, "AbstractMemory", rb_cObject);
#undef INT
#define INT(type) \
    rb_define_method(classMemory, "put_" #type, memory_put_##type, 2); \
    rb_define_method(classMemory, "get_" #type, memory_get_##type, 1); \
    rb_define_method(classMemory, "put_u" #type, memory_put_u##type, 2); \
    rb_define_method(classMemory, "get_u" #type, memory_get_u##type, 1);
    INT(int8);
    INT(int16);
    INT(int32);
    INT(int64);
    rb_define_method(classMemory, "put_float32", memory_put_float32, 2);
    rb_define_method(classMemory, "get_float32", memory_get_float32, 1);
    rb_define_method(classMemory, "put_float64", memory_put_float64, 2);
    rb_define_method(classMemory, "get_float64", memory_get_float64, 1);
}

