
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
autoptr_new(VALUE klass, VALUE other)
{
    AutoPointer* p;
    AbstractMemory* ptr;
    VALUE retval;

    retval = Data_Make_Struct(klass, AutoPointer, autoptr_mark, autoptr_free, p);
    ptr = (AbstractMemory *) DATA_PTR(other);
    p->memory = *ptr;
    p->parent = other;
    return retval;
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
    rb_define_singleton_method(classAutoPointer, "__alloc", autoptr_new, 1);
}