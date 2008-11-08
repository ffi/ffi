#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"
#include "Pointer.h"
#include "MemoryPointer.h"

typedef struct MemoryPointer {
    AbstractMemory memory;
    bool autorelease;
    bool allocated;
} MemoryPointer;

static VALUE memptr_allocate(VALUE self, VALUE size, VALUE count, VALUE clear);
static void memptr_release(MemoryPointer* ptr);

VALUE rb_FFI_MemoryPointer_class;
static VALUE classMemoryPointer = Qnil;

static VALUE
memptr_allocate(VALUE self, VALUE size, VALUE count, VALUE clear)
{
    MemoryPointer* p;
    VALUE retval;
    caddr_t memory;
    unsigned long msize = NUM2LONG(size) * (count == Qnil ? 1 : NUM2LONG(count));
    memory = malloc(msize);
    if (memory == NULL) {
        rb_raise(rb_eNoMemError, "Failed to allocate memory size=%ld bytes", msize);
    }
    retval = Data_Make_Struct(classMemoryPointer, MemoryPointer, NULL, memptr_release, p);
    p->autorelease = true;
    p->memory.size = msize;
    p->memory.address = memory;
    p->allocated = true;
    if (TYPE(clear) == T_TRUE && p->memory.size > 0) {
        memset(p->memory.address, 0, p->memory.size);
    }
    return retval;
}


static VALUE
memptr_inspect(VALUE self)
{
    MemoryPointer* ptr = (MemoryPointer *) DATA_PTR(self);
    char tmp[100];
    snprintf(tmp, sizeof(tmp), "#<MemoryPointer address=%p size=%lu>", ptr->memory.address, ptr->memory.size);
    return rb_str_new2(tmp);
}

static VALUE
memptr_free(VALUE self)
{
    MemoryPointer* ptr = (MemoryPointer *) DATA_PTR(self);
    if (ptr->allocated) {
        if (ptr->memory.address != NULL) {
            free(ptr->memory.address);
            ptr->memory.address = NULL;
        }
        ptr->allocated = false;
    }
    return self;
}

static VALUE
memptr_autorelease(VALUE self, VALUE autorelease)
{
    MemoryPointer* ptr = (MemoryPointer *) DATA_PTR(self);
    ptr->autorelease = autorelease == Qtrue;
    return self;
}

static void
memptr_release(MemoryPointer* ptr)
{
    if (ptr->autorelease && ptr->allocated && ptr->memory.address != NULL) {
        free(ptr->memory.address);
        ptr->memory.address = NULL;
    }
    xfree(ptr);

}

void
rb_FFI_MemoryPointer_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    rb_FFI_MemoryPointer_class = classMemoryPointer = rb_define_class_under(moduleFFI, "MemoryPointer", rb_FFI_Pointer_class);
    rb_define_singleton_method(classMemoryPointer, "__allocate", memptr_allocate, 3);
    rb_define_method(classMemoryPointer, "inspect", memptr_inspect, 0);
    rb_define_method(classMemoryPointer, "autorelease=", memptr_autorelease, 1);
    rb_define_method(classMemoryPointer, "free", memptr_free, 0);
}
