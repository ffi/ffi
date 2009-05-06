#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"
#include "Pointer.h"

typedef struct Pointer {
    AbstractMemory memory;
    VALUE parent;
} Pointer;

#define POINTER(obj) rb_FFI_AbstractMemory_cast((obj), rb_FFI_Pointer_class)

VALUE rb_FFI_Pointer_class;
static VALUE classPointer = Qnil;
static void ptr_mark(Pointer* ptr);

VALUE
rb_FFI_Pointer_new(void* addr)
{
    Pointer* p;
    VALUE obj;

    if (addr == NULL) {
        return rb_FFI_NullPointer_singleton;
    }

    obj = Data_Make_Struct(classPointer, Pointer, NULL, -1, p);
    p->memory.address = addr;
    p->memory.size = LONG_MAX;
    p->memory.ops = &rb_FFI_AbstractMemory_ops;
    p->parent = Qnil;

    return obj;
}

static VALUE
ptr_allocate(VALUE klass)
{
    Pointer* p;
    VALUE obj;

    obj = Data_Make_Struct(classPointer, Pointer, NULL, -1, p);
    p->parent = Qnil;

    return obj;
}

static VALUE
ptr_plus(VALUE self, VALUE offset)
{
    AbstractMemory* ptr;
    Pointer* p;
    VALUE retval;
    long off = NUM2LONG(offset);

    Data_Get_Struct(self, AbstractMemory, ptr);
    checkBounds(ptr, off, 1);

    retval = Data_Make_Struct(classPointer, Pointer, ptr_mark, -1, p);

    p->memory.address = ptr->address + off;
    p->memory.size = ptr->size == LONG_MAX ? LONG_MAX : ptr->size - off;
    p->memory.ops = &rb_FFI_AbstractMemory_ops;
    p->parent = self;

    return retval;
}

static VALUE
ptr_inspect(VALUE self)
{
    Pointer* ptr;
    char tmp[100];

    Data_Get_Struct(self, Pointer, ptr);
    snprintf(tmp, sizeof(tmp), "#<Native Pointer address=%p>", ptr->memory.address);

    return rb_str_new2(tmp);
}

static VALUE
ptr_null_p(VALUE self)
{
    Pointer* ptr;

    Data_Get_Struct(self, Pointer, ptr);

    return ptr->memory.address == NULL ? Qtrue : Qfalse;
}

static VALUE
ptr_equals(VALUE self, VALUE other)
{
    Pointer* ptr;
    
    Data_Get_Struct(self, Pointer, ptr);

    return ptr->memory.address == POINTER(other)->address ? Qtrue : Qfalse;
}

static VALUE
ptr_address(VALUE self)
{
    Pointer* ptr;
    
    Data_Get_Struct(self, Pointer, ptr);

    return ULL2NUM((uintptr_t) ptr->memory.address);
}

static void
ptr_mark(Pointer* ptr)
{
    rb_gc_mark(ptr->parent);
}

void
rb_FFI_Pointer_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    rb_FFI_Pointer_class = classPointer = rb_define_class_under(moduleFFI, "Pointer", rb_FFI_AbstractMemory_class);
    rb_global_variable(&classPointer);
    rb_global_variable(&rb_FFI_Pointer_class);

    rb_define_alloc_func(classPointer, ptr_allocate);
    rb_define_method(classPointer, "inspect", ptr_inspect, 0);
    rb_define_method(classPointer, "+", ptr_plus, 1);
    rb_define_method(classPointer, "null?", ptr_null_p, 0);
    rb_define_method(classPointer, "address", ptr_address, 0);
    rb_define_method(classPointer, "==", ptr_equals, 1);
}
