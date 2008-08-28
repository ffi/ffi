#include <stdbool.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"

typedef struct MemoryPointer {
    AbstractMemory memory;
    bool autorelease;
} MemoryPointer;

static VALUE memptr_allocate(VALUE self, VALUE size, VALUE clear);
static void memptr_free(MemoryPointer* ptr);
static void memptr_mark(MemoryPointer* ptr);

static VALUE classMemoryPointer = Qnil;


static VALUE
memptr_allocate(VALUE self, VALUE size, VALUE clear)
{
    MemoryPointer* p;
    
    p = ALLOC(MemoryPointer);
    memset(p, 0, sizeof(*p));
    p->autorelease = true;
    p->memory.size = NUM2ULONG(size);
    p->memory.address = malloc(p->memory.size);

    if (p->memory.address == NULL) {
        int size = p->memory.size;
        xfree(p);
        rb_raise(rb_eNoMemError, "Failed to allocate memory size=%u bytes", size);
    }
    if (TYPE(clear) == T_TRUE) {
        memset(p->memory.address, 0, p->memory.size);
    }
    return Data_Wrap_Struct(classMemoryPointer, memptr_mark, memptr_free, p);
}

static void
memptr_free(MemoryPointer* ptr)
{
    if (ptr->autorelease) {
        free(ptr->memory.address);
    }
    xfree(ptr);

}
static void
memptr_mark(MemoryPointer* ptr)
{
    
}
void
rb_FFI_MemoryPointer_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    classMemoryPointer = rb_define_class_under(moduleFFI, "MemoryPointer", rb_FFI_AbstractMemory_class);
    rb_define_singleton_method(classMemoryPointer, "__allocate", memptr_allocate, 2);
}
