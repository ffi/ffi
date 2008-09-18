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

VALUE rb_FFI_Buffer_class;
static VALUE classBuffer = Qnil;

static VALUE
buffer_allocate(VALUE self, VALUE size, VALUE count, VALUE clear)
{
    Buffer* p;

    p = ALLOC(Buffer);
    memset(p, 0, sizeof(*p));
    p->memory.size = NUM2LONG(size) * (count == Qnil ? 1 : NUM2LONG(count));
    p->memory.address = p->memory.size > 0 ? malloc(p->memory.size) : NULL;
    p->parent = Qnil;

    if (p->memory.address == NULL) {
        int size = p->memory.size;
        xfree(p);
        rb_raise(rb_eNoMemError, "Failed to allocate memory size=%u bytes", size);
    }
    if (TYPE(clear) == T_TRUE) {
        memset(p->memory.address, 0, p->memory.size);
    }
    return Data_Wrap_Struct(classBuffer, buffer_mark, buffer_release, p);
}

static VALUE
buffer_plus(VALUE self, VALUE offset)
{
    Buffer* ptr = (Buffer *) DATA_PTR(self);
    Buffer* p;
    long off = NUM2LONG(offset);

    checkBounds(&ptr->memory, off, 1);
    p = ALLOC(Buffer);
    memset(p, 0, sizeof(*p));
    p->memory.address = ptr->memory.address + off;;
    p->memory.size = ptr->memory.size - off;;
    p->parent = self;
    return Data_Wrap_Struct(classBuffer, buffer_mark, buffer_release, p);
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
    if (ptr->parent != Qnil) {
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
    rb_FFI_Buffer_class = classBuffer = rb_define_class_under(moduleFFI, "Buffer", rb_FFI_AbstractMemory_class);
    rb_define_singleton_method(classBuffer, "__alloc_inout", buffer_allocate, 3);
    rb_define_singleton_method(classBuffer, "__alloc_out", buffer_allocate, 3);
    rb_define_singleton_method(classBuffer, "__alloc_in", buffer_allocate, 3);
    rb_define_method(classBuffer, "inspect", buffer_inspect, 0);
    rb_define_method(classBuffer, "+", buffer_plus, 1);
}

