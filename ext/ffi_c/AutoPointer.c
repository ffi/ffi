
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

VALUE rb_FFI_AutoPointer_class;
static VALUE classAutoPointer = Qnil;
static void autoptr_mark(AutoPointer* ptr);
static void autoptr_free(AutoPointer* ptr);

static VALUE
autoptr_allocate(VALUE klass)
{
    AutoPointer* p;
    VALUE obj = Data_Make_Struct(klass, AutoPointer, autoptr_mark, autoptr_free, p);
    p->parent = Qnil;
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
    if (ptr->parent != Qnil) {
        rb_gc_mark(ptr->parent);
    }
}
static void
autoptr_free(AutoPointer* ptr)
{
    xfree(ptr);
}

void
rb_FFI_AutoPointer_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    rb_FFI_AutoPointer_class = classAutoPointer = rb_define_class_under(moduleFFI, "AutoPointer", rb_FFI_Pointer_class);
    rb_define_alloc_func(classAutoPointer, autoptr_allocate);
    rb_define_protected_method(classAutoPointer, "parent=", autoptr_set_parent, 1);
}
