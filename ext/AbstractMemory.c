#include <sys/types.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"
#include "MemoryPointer.h"

static VALUE memory_put_float32(VALUE self, VALUE offset, VALUE value);
static VALUE memory_get_float32(VALUE self, VALUE offset);
static VALUE memory_put_float64(VALUE self, VALUE offset, VALUE value);
static VALUE memory_get_float64(VALUE self, VALUE offset);
static VALUE memory_put_pointer(VALUE self, VALUE offset, VALUE value);
static VALUE memory_get_pointer(VALUE self, VALUE offset);

static inline caddr_t memory_address(VALUE self);

VALUE rb_FFI_AbstractMemory_class = Qnil;
static VALUE classMemory = Qnil;

#define ADDRESS(self, offset) (memory_address((self)) + NUM2ULONG(offset))

#define NUM_OP(name, type, toNative, fromNative) \
static VALUE memory_put_##name(VALUE self, VALUE offset, VALUE value); \
static VALUE \
memory_put_##name(VALUE self, VALUE offset, VALUE value) \
{ \
    *(type *) ADDRESS(self, offset) = (type) toNative(value); \
    return self; \
} \
static VALUE memory_get_##name(VALUE self, VALUE offset); \
static VALUE \
memory_get_##name(VALUE self, VALUE offset) \
{ \
    return fromNative(*(type *) ADDRESS(self, offset)); \
} \
static VALUE memory_put_array_of_##name(VALUE self, VALUE offset, VALUE ary); \
static VALUE \
memory_put_array_of_##name(VALUE self, VALUE offset, VALUE ary) \
{ \
    long count = RARRAY(ary)->len; \
    long off = NUM2LONG(offset); \
    AbstractMemory* memory = (AbstractMemory *) DATA_PTR(self); \
    caddr_t address = memory->address; \
    long i; \
    for (i = 0; i < count; i++) { \
        *((type *)(address + off + (i * sizeof(type)))) = (type) toNative(rb_ary_entry(ary, i)); \
    } \
    return self; \
} \
static VALUE memory_get_array_of_##name(VALUE self, VALUE offset, VALUE length); \
static VALUE \
memory_get_array_of_##name(VALUE self, VALUE offset, VALUE length) \
{ \
    long count = NUM2LONG(length); \
    long off = NUM2LONG(offset); \
    AbstractMemory* memory = (AbstractMemory *) DATA_PTR(self); \
    caddr_t address = memory->address; \
    long last = off + count; \
    long i; \
    VALUE retVal = rb_ary_new2(count); \
    for (i = off; i < last; ++i) { \
        rb_ary_push(retVal, fromNative(*((type *) (address + (i * sizeof(type)))))); \
    } \
    return retVal; \
}

#define INT(type, toNative, fromNative) NUM_OP(type, type##_t, toNative, fromNative); \
        NUM_OP(u##type, u_##type##_t, toNative, fromNative)
INT(int8, NUM2INT, INT2FIX);
INT(int16, NUM2INT, INT2FIX);
INT(int32, NUM2INT, INT2FIX);
NUM_OP(int64, int64_t, NUM2LL, LL2NUM);
NUM_OP(uint64, u_int64_t, NUM2ULL, ULL2NUM);
NUM_OP(float32, float, NUM2DBL, rb_float_new);
NUM_OP(float64, double, NUM2DBL, rb_float_new);

static VALUE
memory_put_pointer(VALUE self, VALUE offset, VALUE value)
{
    if (rb_obj_is_kind_of(value, rb_FFI_MemoryPointer_class)) {
        *(caddr_t *) ADDRESS(self, offset) = memory_address(value);
    } else if (TYPE(value) == T_NIL) {
        *(caddr_t *) ADDRESS(self, offset) = NULL;
    } else if (TYPE(value) == T_FIXNUM) {
        *(uintptr_t *) ADDRESS(self, offset) = (uintptr_t) FIX2INT(value);
    } else if (TYPE(value) == T_BIGNUM) {
        *(uintptr_t *) ADDRESS(self, offset) = (uintptr_t) NUM2ULL(value);
    } else {
        rb_raise(rb_eArgError, "value is not a pointer");
    }
    return self;
}

static VALUE
memory_get_pointer(VALUE self, VALUE offset)
{
    return rb_FFI_MemoryPointer_new(*(caddr_t *) ADDRESS(self, offset));
}

static VALUE
memory_clear(VALUE self)
{
    AbstractMemory* ptr = (AbstractMemory *) DATA_PTR(self);
    memset(ptr->address, 0, ptr->size);
    return self;
}

static VALUE
memory_size(VALUE self) 
{
    return LONG2FIX(((AbstractMemory *) DATA_PTR(self))->size);
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
    rb_define_method(classMemory, "get_u" #type, memory_get_u##type, 1); \
    rb_define_method(classMemory, "put_array_of_" #type, memory_put_array_of_##type, 2); \
    rb_define_method(classMemory, "get_array_of_" #type, memory_get_array_of_##type, 2); \
    rb_define_method(classMemory, "put_array_of_u" #type, memory_put_array_of_u##type, 2); \
    rb_define_method(classMemory, "get_array_of_u" #type, memory_get_array_of_u##type, 2);
    
    INT(int8);
    INT(int16);
    INT(int32);
    INT(int64);
    
#define ALIAS(name, old) \
    rb_define_alias(classMemory, "put_" #name, "put_" #old); \
    rb_define_alias(classMemory, "get_" #name, "get_" #old); \
    rb_define_alias(classMemory, "put_u" #name, "put_u" #old); \
    rb_define_alias(classMemory, "get_u" #name, "get_u" #old); \
    rb_define_alias(classMemory, "put_array_of_" #name, "put_array_of_" #old); \
    rb_define_alias(classMemory, "get_array_of_" #name, "get_array_of_" #old); \
    rb_define_alias(classMemory, "put_array_of_u" #name, "put_array_of_u" #old); \
    rb_define_alias(classMemory, "get_array_of_u" #name, "get_array_of_u" #old);
    
    ALIAS(char, int8);
    ALIAS(short, int16);
    ALIAS(int, int32);
    ALIAS(long_long, int64);
    
    if (sizeof(long) == 4) {
        rb_define_alias(classMemory, "put_long", "put_int32");
        rb_define_alias(classMemory, "put_ulong", "put_uint32");
        rb_define_alias(classMemory, "get_long", "get_int32");
        rb_define_alias(classMemory, "get_ulong", "get_uint32");
    } else {
        rb_define_alias(classMemory, "put_long", "put_int64");
        rb_define_alias(classMemory, "put_ulong", "put_uint64");
        rb_define_alias(classMemory, "get_long", "get_int64");
        rb_define_alias(classMemory, "get_ulong", "get_uint64");
    }
    rb_define_method(classMemory, "put_float32", memory_put_float32, 2);
    rb_define_method(classMemory, "get_float32", memory_get_float32, 1);
    rb_define_method(classMemory, "put_array_of_float32", memory_put_array_of_float32, 2);
    rb_define_method(classMemory, "get_array_of_float32", memory_get_array_of_float32, 2);
    rb_define_method(classMemory, "put_float64", memory_put_float64, 2);
    rb_define_method(classMemory, "get_float64", memory_get_float64, 1);
    rb_define_method(classMemory, "put_array_of_float64", memory_put_array_of_float64, 2);
    rb_define_method(classMemory, "get_array_of_float64", memory_get_array_of_float64, 2);
    rb_define_method(classMemory, "put_pointer", memory_put_pointer, 2);
    rb_define_method(classMemory, "get_pointer", memory_get_pointer, 1);

    rb_define_method(classMemory, "clear", memory_clear, 0);
    rb_define_method(classMemory, "total", memory_size, 0);
}

