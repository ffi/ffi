#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"

typedef struct Buffer {
    AbstractMemory memory;
    char* storage; /* start of malloc area */
    int type_size;
    VALUE parent;
} Buffer;

static VALUE buffer_allocate(VALUE klass);
static VALUE buffer_initialize(int argc, VALUE* argv, VALUE self);
static void buffer_release(Buffer* ptr);
static void buffer_mark(Buffer* ptr);
static VALUE buffer_free(VALUE self);

static VALUE BufferClass = Qnil;
#define BUFFER(obj)  ((Buffer *) rbffi_AbstractMemory_Cast((obj), BufferClass))

static VALUE
buffer_allocate(VALUE klass)
{
    Buffer* buffer;
    VALUE obj;

    obj = Data_Make_Struct(klass, Buffer, NULL, buffer_release, buffer);
    buffer->parent = Qnil;
    buffer->memory.ops = &rbffi_AbstractMemoryOps;

    return obj;
}

static void
buffer_release(Buffer* ptr)
{
    if (ptr->storage != NULL) {
        free(ptr->storage);
        ptr->storage = NULL;
    }
    
    xfree(ptr);
}

static VALUE
buffer_initialize(int argc, VALUE* argv, VALUE self)
{
    VALUE size = Qnil, count = Qnil, clear = Qnil;
    Buffer* p;
    int nargs;

    Data_Get_Struct(self, Buffer, p);

    nargs = rb_scan_args(argc, argv, "12", &size, &count, &clear);
    p->type_size = rbffi_type_size(size);
    p->memory.size = p->type_size * (nargs > 1 ? NUM2LONG(count) : 1);

    p->storage = malloc(p->memory.size + 7);
    if (p->storage == NULL) {
        rb_raise(rb_eNoMemError, "Failed to allocate memory size=%lu bytes", p->memory.size);
    }

    /* ensure the memory is aligned on at least a 8 byte boundary */
    p->memory.address = (void *) (((uintptr_t) p->storage + 0x7) & (uintptr_t) ~0x7UL);
    
    if (nargs > 2 && (RTEST(clear) || clear == Qnil) && p->memory.size > 0) {
        memset(p->memory.address, 0, p->memory.size);
    }

    if (rb_block_given_p()) {
        return rb_ensure(rb_yield, self, buffer_free, self);
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

    retval = Data_Make_Struct(BufferClass, Buffer, buffer_mark, -1, p);
    p->memory.address = ptr->memory.address + off;
    p->memory.size = ptr->memory.size - off;
    p->memory.ops = &rbffi_AbstractMemoryOps;
    p->parent = self;

    return retval;
}

static VALUE
buffer_aref(VALUE self, VALUE offset)
{
    Buffer* ptr;

    Data_Get_Struct(self, Buffer, ptr);
    return buffer_plus(self, INT2FIX(ptr->type_size * NUM2INT(offset)));
}

static VALUE
buffer_type_size(VALUE self)
{
    Buffer* ptr;

    Data_Get_Struct(self, Buffer, ptr);
    return INT2NUM(ptr->type_size);
}

static VALUE
buffer_size(VALUE self)
{
    return INT2NUM(BUFFER(self)->memory.size);
}

static VALUE
buffer_inspect(VALUE self)
{
    char tmp[100];

    snprintf(tmp, sizeof(tmp), "#<Buffer size=%ld>", BUFFER(self)->memory.size);
    
    return rb_str_new2(tmp);
}

/* Only used to free the buffer if the yield in the initializer throws an exception */
static VALUE
buffer_free(VALUE self)
{
    Buffer* ptr;

    Data_Get_Struct(self, Buffer, ptr);
    if (ptr->storage != NULL) {
        free(ptr->storage);
        ptr->storage = NULL;
    }

    return self;
}

static void
buffer_mark(Buffer* ptr)
{
    rb_gc_mark(ptr->parent);
}

void
rbffi_Buffer_Init(VALUE moduleFFI)
{
    BufferClass = rb_define_class_under(moduleFFI, "Buffer", rbffi_AbstractMemoryClass);

    rb_global_variable(&BufferClass);
    rb_define_alloc_func(BufferClass, buffer_allocate);

    rb_define_singleton_method(BufferClass, "alloc_inout", buffer_alloc_inout, -1);
    rb_define_singleton_method(BufferClass, "alloc_out", buffer_alloc_inout, -1);
    rb_define_singleton_method(BufferClass, "alloc_in", buffer_alloc_inout, -1);
    
    rb_define_method(BufferClass, "initialize", buffer_initialize, -1);
    rb_define_method(BufferClass, "inspect", buffer_inspect, 0);
    rb_define_method(BufferClass, "size", buffer_size, 0);
    rb_define_alias(BufferClass, "length", "size");
    rb_define_alias(BufferClass, "total", "size");
    rb_define_method(BufferClass, "type_size", buffer_type_size, 0);
    rb_define_method(BufferClass, "[]", buffer_aref, 1);
    rb_define_method(BufferClass, "+", buffer_plus, 1);
}
