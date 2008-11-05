#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"

typedef struct Buffer {
    AbstractMemory memory;
    VALUE parent;
} Buffer;

static VALUE buffer_allocate(VALUE self, VALUE size, VALUE count, VALUE clear);
static void buffer_release(Buffer* ptr);
static void buffer_mark(Buffer* ptr);

static VALUE classBuffer = Qnil;

static VALUE
buffer_allocate(VALUE self, VALUE size, VALUE count, VALUE clear)
{
    Buffer* p;
    VALUE retval;
    unsigned long msize = NUM2LONG(size) * (count == Qnil ? 1 : NUM2LONG(count));
    caddr_t memory;

    memory = malloc(msize);
    if (memory == NULL) {
        rb_raise(rb_eNoMemError, "Failed to allocate memory size=%u bytes", msize);
    }
    retval = Data_Make_Struct(classBuffer, Buffer, buffer_mark, buffer_release, p);
    p->memory.size = msize;
    p->memory.address = memory;
    p->parent = Qnil;
    if (TYPE(clear) == T_TRUE && p->memory.size > 0) {
        memset(p->memory.address, 0, p->memory.size);
    }
    return retval;
}

static VALUE
buffer_plus(VALUE self, VALUE offset)
{
    Buffer* ptr = (Buffer *) DATA_PTR(self);
    Buffer* p;
    VALUE retval;
    long off = NUM2LONG(offset);

    checkBounds(&ptr->memory, off, 1);
    retval = Data_Make_Struct(classBuffer, Buffer, buffer_mark, buffer_release, p);
    p->memory.address = ptr->memory.address + off;;
    p->memory.size = ptr->memory.size - off;
    p->parent = self;
    return retval;
}

static VALUE
buffer_inspect(VALUE self)
{
    Buffer* ptr = (Buffer *) DATA_PTR(self);
    char tmp[100];
    snprintf(tmp, sizeof(tmp), "#<Buffer size=%ld>", ptr->memory.size);
    return rb_str_new2(tmp);
}

static void
buffer_release(Buffer* ptr)
{
    if (ptr->parent == Qnil && ptr->memory.address != NULL) {
        free(ptr->memory.address);
    }
    xfree(ptr);

}
static void
buffer_mark(Buffer* ptr)
{
    if (ptr->parent != Qnil) {
        rb_gc_mark(ptr->parent);
    }
}

void
rb_FFI_Buffer_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    classBuffer = rb_define_class_under(moduleFFI, "Buffer", rb_FFI_AbstractMemory_class);
    rb_define_singleton_method(classBuffer, "__alloc_inout", buffer_allocate, 3);
    rb_define_singleton_method(classBuffer, "__alloc_out", buffer_allocate, 3);
    rb_define_singleton_method(classBuffer, "__alloc_in", buffer_allocate, 3);
    rb_define_method(classBuffer, "inspect", buffer_inspect, 0);
    rb_define_method(classBuffer, "+", buffer_plus, 1);
}

