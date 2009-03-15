#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"

typedef struct Buffer {
    AbstractMemory memory;
    void* address; /* The C heap address */
    VALUE parent;
} Buffer;

static VALUE buffer_allocate(VALUE klass);
static VALUE buffer_initialize(int argc, VALUE* argv, VALUE self);
static void buffer_release(Buffer* ptr);
static void buffer_mark(Buffer* ptr);

static VALUE classBuffer = Qnil;
#define BUFFER(obj)  ((Buffer *) rb_FFI_AbstractMemory_cast((obj), classBuffer))

static VALUE
buffer_allocate(VALUE klass)
{
    Buffer* buffer;
    return Data_Make_Struct(klass, Buffer, buffer_mark, buffer_release, buffer);
}

static VALUE
buffer_initialize(int argc, VALUE* argv, VALUE self)
{
    VALUE size = Qnil, count = Qnil, clear = Qnil;
    Buffer* p;
    unsigned long msize;
    void* memory;
    int nargs;

    nargs = rb_scan_args(argc, argv, "12", &size, &count, &clear);
    msize = rb_FFI_type_size(size) * (nargs > 1 ? NUM2LONG(count) : 1);
    memory = malloc(msize + 7);
    if (memory == NULL) {
        rb_raise(rb_eNoMemError, "Failed to allocate memory size=%lu bytes", msize);
    }
    Data_Get_Struct(self, Buffer, p);
    p->address = memory;
    p->memory.size = msize;
    /* ensure the memory is aligned on at least a 8 byte boundary */
    p->memory.address = (void *) (((uintptr_t) memory + 0x7) & (uintptr_t) ~0x7UL);
    p->parent = Qnil;
    if (nargs > 2 && RTEST(clear) && p->memory.size > 0) {
        memset(p->memory.address, 0, p->memory.size);
    }
    return self;
}

static VALUE
buffer_alloc_inout(int argc, VALUE* argv, VALUE klass)
{
    return buffer_initialize(argc, argv, buffer_allocate(klass));
}

static VALUE
buffer_plus(VALUE self, VALUE offset)
{
    Buffer* ptr = BUFFER(self);
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
    char tmp[100];
    snprintf(tmp, sizeof(tmp), "#<Buffer size=%ld>", BUFFER(self)->memory.size);
    return rb_str_new2(tmp);
}

static void
buffer_release(Buffer* ptr)
{
    if (ptr->parent == Qnil && ptr->address != NULL) {
        free(ptr->address);
        ptr->address = NULL;
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
    rb_define_alloc_func(classBuffer, buffer_allocate);

    rb_define_singleton_method(classBuffer, "alloc_inout", buffer_alloc_inout, -1);
    rb_define_singleton_method(classBuffer, "alloc_out", buffer_alloc_inout, -1);
    rb_define_singleton_method(classBuffer, "alloc_in", buffer_alloc_inout, -1);
    
    rb_define_method(classBuffer, "initialize", buffer_initialize, -1);
    rb_define_method(classBuffer, "inspect", buffer_inspect, 0);
    rb_define_method(classBuffer, "+", buffer_plus, 1);
}

