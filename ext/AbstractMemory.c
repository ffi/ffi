#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>
#include <ruby.h>
#include "rbffi.h"
#include "compat.h"
#include "AbstractMemory.h"
#include "Pointer.h"
#include "Callback.h"

static VALUE memory_put_float32(VALUE self, VALUE offset, VALUE value);
static VALUE memory_get_float32(VALUE self, VALUE offset);
static VALUE memory_put_float64(VALUE self, VALUE offset, VALUE value);
static VALUE memory_get_float64(VALUE self, VALUE offset);
static VALUE memory_put_pointer(VALUE self, VALUE offset, VALUE value);
static VALUE memory_get_pointer(VALUE self, VALUE offset);

static inline caddr_t memory_address(VALUE self);
VALUE rb_FFI_AbstractMemory_class = Qnil;
static VALUE classMemory = Qnil;
static ID to_ptr = 0;

#define ADDRESS(self, offset) (memory_address((self)) + NUM2ULONG(offset))
#ifndef RARRAY_LEN
#  define RARRAY_LEN(ary) RARRAY(ary)->len
#endif

#define NUM_OP(name, type, toNative, fromNative) \
static VALUE memory_put_##name(VALUE self, VALUE offset, VALUE value); \
static VALUE \
memory_put_##name(VALUE self, VALUE offset, VALUE value) \
{ \
    long off = NUM2LONG(offset); \
    AbstractMemory* memory = (AbstractMemory *) DATA_PTR(self); \
    type tmp = (type) toNative(value); \
    checkBounds(memory, off, sizeof(type)); \
    memcpy(memory->address + off, &tmp, sizeof(tmp)); \
    return self; \
} \
static VALUE memory_get_##name(VALUE self, VALUE offset); \
static VALUE \
memory_get_##name(VALUE self, VALUE offset) \
{ \
    long off = NUM2LONG(offset); \
    AbstractMemory* memory = (AbstractMemory *) DATA_PTR(self); \
    type tmp; \
    checkBounds(memory, off, sizeof(type)); \
    memcpy(&tmp, memory->address + off, sizeof(tmp)); \
    return fromNative(tmp); \
} \
static VALUE memory_put_array_of_##name(VALUE self, VALUE offset, VALUE ary); \
static VALUE \
memory_put_array_of_##name(VALUE self, VALUE offset, VALUE ary) \
{ \
    long count = RARRAY_LEN(ary); \
    long off = NUM2LONG(offset); \
    AbstractMemory* memory = (AbstractMemory *) DATA_PTR(self); \
    caddr_t address = memory->address; \
    long i; \
    checkBounds(memory, off, count * sizeof(type)); \
    for (i = 0; i < count; i++) { \
        type tmp = (type) toNative(rb_ary_entry(ary, i)); \
        memcpy(address + off + (i * sizeof(type)), &tmp, sizeof(tmp)); \
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
    checkBounds(memory, off, count * sizeof(type)); \
    VALUE retVal = rb_ary_new2(count); \
    for (i = off; i < last; ++i) { \
        type tmp; \
        memcpy(&tmp, address + (i * sizeof(type)), sizeof(tmp)); \
        rb_ary_push(retVal, fromNative(tmp)); \
    } \
    return retVal; \
}

NUM_OP(int8, int8_t, NUM2INT, INT2NUM);
NUM_OP(uint8, u_int8_t, NUM2UINT, UINT2NUM);
NUM_OP(int16, int16_t, NUM2INT, INT2NUM);
NUM_OP(uint16, u_int16_t, NUM2UINT, UINT2NUM);
NUM_OP(int32, int32_t, NUM2INT, INT2NUM);
NUM_OP(uint32, u_int32_t, NUM2UINT, UINT2NUM);
NUM_OP(int64, int64_t, NUM2LL, LL2NUM);
NUM_OP(uint64, u_int64_t, NUM2ULL, ULL2NUM);
NUM_OP(float32, float, NUM2DBL, rb_float_new);
NUM_OP(float64, double, NUM2DBL, rb_float_new);

static VALUE
memory_put_pointer(VALUE self, VALUE offset, VALUE value)
{ 
    AbstractMemory* memory = (AbstractMemory *) DATA_PTR(self);
    long off = NUM2LONG(offset);
    const int type = TYPE(value);
    checkBounds(memory, off, sizeof(void *));

    if (rb_obj_is_kind_of(value, rb_FFI_Pointer_class) && type == T_DATA) {
        void* tmp = memory_address(value);
        memcpy(memory->address + off, &tmp, sizeof(tmp));
    } else if (type == T_NIL) {
        void* tmp = NULL;
        memcpy(memory->address + off, &tmp, sizeof(tmp));
    } else if (type == T_FIXNUM) {
        uintptr_t tmp = (uintptr_t) FIX2INT(value);
        memcpy(memory->address + off, &tmp, sizeof(tmp));
    } else if (type == T_BIGNUM) {
        uintptr_t tmp = (uintptr_t) NUM2ULL(value);
        memcpy(memory->address + off, &tmp, sizeof(tmp));
    } else if (rb_respond_to(value, to_ptr)) {
        VALUE ptr = rb_funcall2(value, to_ptr, 0, NULL);
        if (rb_obj_is_kind_of(ptr, rb_FFI_Pointer_class) && TYPE(ptr) == T_DATA) {
            void* tmp = memory_address(ptr);
            memcpy(memory->address + off, &tmp, sizeof(tmp));
        } else {
            rb_raise(rb_eArgError, "to_ptr returned an invalid pointer");
        }
    } else {
        rb_raise(rb_eArgError, "value is not a pointer");
    }
    return self;
}

static VALUE
memory_get_pointer(VALUE self, VALUE offset)
{
    AbstractMemory* memory = (AbstractMemory *) DATA_PTR(self);
    long off = NUM2LONG(offset);
    caddr_t tmp;
    checkBounds(memory, off, sizeof(tmp));
    memcpy(&tmp, memory->address + off, sizeof(tmp));
    return rb_FFI_Pointer_new(tmp);
}

static VALUE
memory_put_callback(VALUE self, VALUE offset, VALUE proc, VALUE cbInfo)
{
    AbstractMemory* memory = (AbstractMemory *) DATA_PTR(self);
    long off = NUM2LONG(offset);
    checkBounds(memory, off, sizeof(void *));

    if (rb_obj_is_kind_of(proc, rb_cProc)) {
        VALUE callback = rb_FFI_NativeCallback_for_proc(proc, cbInfo);
        void* code = ((NativeCallback *) DATA_PTR(callback))->code;
        memcpy(memory->address + off, &code, sizeof(code));
    } else {
        rb_raise(rb_eArgError, "parameter is not a proc");
    }

    return self;
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

static VALUE
memory_get_string(int argc, VALUE* argv, VALUE self)
{
    VALUE length = Qnil, offset = Qnil;
    AbstractMemory* ptr = (AbstractMemory *) DATA_PTR(self);
    long off, len;
    caddr_t end;
    int nargs = rb_scan_args(argc, argv, "11", &offset, &length);

    off = NUM2LONG(offset);
    len = nargs > 1 && length != Qnil ? NUM2LONG(length) : (ptr->size - off);
    checkBounds(ptr, off, len);
    end = memchr(ptr->address + off, 0, len);
    return rb_tainted_str_new((char *) ptr->address + off,
            (end != NULL ? end - ptr->address - off : len));
}

static VALUE
memory_put_string(VALUE self, VALUE offset, VALUE str)
{
    AbstractMemory* ptr = (AbstractMemory *) DATA_PTR(self);
    long off, len;

    off = NUM2LONG(offset);
    len = RSTRING_LEN(str);
    checkBounds(ptr, off, len);
    if (rb_safe_level() >= 1 && OBJ_TAINTED(str)) {
        rb_raise(rb_eSecurityError, "Writing unsafe string to memory");
    }
    memcpy(ptr->address + off, RSTRING_PTR(str), len);
    *((char *) ptr->address + off + len) = '\0';
    return self;
}

static VALUE
memory_get_bytes(VALUE self, VALUE offset, VALUE length)
{
    AbstractMemory* ptr = (AbstractMemory *) DATA_PTR(self);
    long off, len;
    
    off = NUM2LONG(offset);
    len = NUM2LONG(length);
    checkBounds(ptr, off, len);
    return rb_tainted_str_new((char *) ptr->address + off, len);
}

static VALUE
memory_put_bytes(int argc, VALUE* argv, VALUE self)
{
    AbstractMemory* ptr = (AbstractMemory *) DATA_PTR(self);
    VALUE offset = Qnil, str = Qnil, rbIndex = Qnil, rbLength = Qnil;
    long off, len, idx;
    int nargs = rb_scan_args(argc, argv, "22", &offset, &str, &rbIndex, &rbLength);

    off = NUM2LONG(offset);
    idx = nargs > 2 ? NUM2LONG(rbIndex) : 0;
    if (idx < 0) {
        rb_raise(rb_eRangeError, "index canot be less than zero");
    }
    len = nargs > 3 ? NUM2LONG(rbLength) : (RSTRING_LEN(str) - idx);
    if ((idx + len) > RSTRING_LEN(str)) {
        rb_raise(rb_eRangeError, "index+length is greater than size of string");
    }
    checkBounds(ptr, off, len);
    if (rb_safe_level() >= 1 && OBJ_TAINTED(str)) {
        rb_raise(rb_eSecurityError, "Writing unsafe string to memory");
    }
    memcpy(ptr->address + off, RSTRING_PTR(str) + idx, len);
    return self;
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
    rb_define_method(classMemory, "put_callback", memory_put_callback, 3);
    rb_define_method(classMemory, "get_string", memory_get_string, -1);
    rb_define_method(classMemory, "put_string", memory_put_string, 2);
    rb_define_method(classMemory, "get_bytes", memory_get_bytes, 2);
    rb_define_method(classMemory, "put_bytes", memory_put_bytes, -1);

    rb_define_method(classMemory, "clear", memory_clear, 0);
    rb_define_method(classMemory, "total", memory_size, 0);

    to_ptr = rb_intern("to_ptr");
}

