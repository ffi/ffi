
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"
#include "Pointer.h"
#include "AutoPointer.h"

typedef struct AutoPointer {
    AbstractMemory memory;
    VALUE parent;
} AutoPointer;

static void autoptr_mark(AutoPointer* ptr);
static VALUE autoptr_allocate(VALUE klass);
static VALUE autoptr_set_parent(VALUE self, VALUE parent);

VALUE rb_FFI_AutoPointer_class = Qnil;

static VALUE
autoptr_allocate(VALUE klass)
{
    AutoPointer* p;
    VALUE obj = Data_Make_Struct(klass, AutoPointer, autoptr_mark, -1, p);
    p->parent = Qnil;
    p->memory.ops = &rb_FFI_AbstractMemory_ops;

    return obj;
}

static VALUE
autoptr_set_parent(VALUE self, VALUE parent)
{
    AutoPointer* p;
    AbstractMemory* ptr = rb_FFI_AbstractMemory_cast(parent, rb_FFI_Pointer_class);

    Data_Get_Struct(self, AutoPointer, p);
    p->memory = *ptr;
    p->parent = parent;

    return self;
}

static void
autoptr_mark(AutoPointer* ptr)
{
    rb_gc_mark(ptr->parent);
}

void
rb_FFI_AutoPointer_Init(VALUE moduleFFI)
{
    rb_FFI_AutoPointer_class = rb_define_class_under(moduleFFI, "AutoPointer", rb_FFI_Pointer_class);
    rb_global_variable(&rb_FFI_AutoPointer_class);
    
    rb_define_alloc_func(rb_FFI_AutoPointer_class, autoptr_allocate);
    rb_define_protected_method(rb_FFI_AutoPointer_class, "parent=", autoptr_set_parent, 1);
}
