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
    char* storage; /* start of malloc area */
    int type_size;
    bool autorelease;
    bool allocated;
} MemoryPointer;

static VALUE memptr_allocate(VALUE klass);
static void memptr_release(MemoryPointer* ptr);
static VALUE memptr_malloc(VALUE self, long size, long count, bool clear);
static VALUE memptr_free(VALUE self);

VALUE rbffi_MemoryPointerClass;

#define MEMPTR(obj) ((MemoryPointer *) rbffi_AbstractMemory_Cast(obj, rbffi_MemoryPointerClass))

static ID plus_id = 0;

VALUE
rbffi_MemoryPointer_NewInstance(long size, long count, bool clear)
{
    return memptr_malloc(memptr_allocate(rbffi_MemoryPointerClass), size, count, clear);
}

static VALUE
memptr_allocate(VALUE klass)
{
    MemoryPointer* p;
    VALUE obj = Data_Make_Struct(klass, MemoryPointer, NULL, memptr_release, p);
    p->memory.ops = &rbffi_AbstractMemoryOps;
    p->memory.access = MEM_RD | MEM_WR;

    return obj;
}

static VALUE
memptr_initialize(int argc, VALUE* argv, VALUE self)
{
    VALUE size = Qnil, count = Qnil, clear = Qnil;
    int nargs = rb_scan_args(argc, argv, "12", &size, &count, &clear);

    memptr_malloc(self, rbffi_type_size(size), nargs > 1 ? NUM2LONG(count) : 1,
        RTEST(clear) || clear == Qnil);
    
    if (rb_block_given_p()) {
        return rb_ensure(rb_yield, self, memptr_free, self);
    }

    return self;
}

static VALUE
memptr_malloc(VALUE self, long size, long count, bool clear)
{
    MemoryPointer* p;
    unsigned long msize;

    Data_Get_Struct(self, MemoryPointer, p);
    p->type_size = size;

    msize = size * count;

    p->storage = malloc(msize + 7);
    if (p->storage == NULL) {
        rb_raise(rb_eNoMemError, "Failed to allocate memory size=%ld bytes", msize);
    }
    p->autorelease = true;
    p->memory.size = msize;
    /* ensure the memory is aligned on at least a 8 byte boundary */
    p->memory.address = (char *) (((uintptr_t) p->storage + 0x7) & (uintptr_t) ~0x7UL);;
    p->allocated = true;
    if (clear && p->memory.size > 0) {
        memset(p->memory.address, 0, p->memory.size);
    }
    return self;
}

static VALUE
memptr_inspect(VALUE self)
{
    MemoryPointer* ptr = MEMPTR(self);
    char tmp[100];
    snprintf(tmp, sizeof(tmp), "#<MemoryPointer address=%p size=%lu>", ptr->memory.address, ptr->memory.size);
    return rb_str_new2(tmp);
}

static VALUE
memptr_type_size(VALUE self)
{
    return INT2FIX(MEMPTR(self)->type_size);
}

static VALUE
memptr_aref(VALUE self, VALUE which)
{
    MemoryPointer* ptr = MEMPTR(self);
    VALUE offset = INT2NUM(ptr->type_size * NUM2INT(which));
    return rb_funcall2(self, plus_id, 1, &offset);
}

static VALUE
memptr_free(VALUE self)
{
    MemoryPointer* ptr = MEMPTR(self);
    if (ptr->allocated) {
        if (ptr->storage != NULL) {
            free(ptr->storage);
            ptr->storage = NULL;
        }
        ptr->allocated = false;
    }
    return self;
}

static VALUE
memptr_autorelease(VALUE self, VALUE autorelease)
{
    MEMPTR(self)->autorelease = autorelease == Qtrue;
    return self;
}

static void
memptr_release(MemoryPointer* ptr)
{
    if (ptr->autorelease && ptr->allocated && ptr->storage != NULL) {
        free(ptr->storage);
        ptr->storage = NULL;
    }
    xfree(ptr);
}

void
rbffi_MemoryPointer_Init(VALUE moduleFFI)
{
    rbffi_MemoryPointerClass = rb_define_class_under(moduleFFI, "MemoryPointer", rbffi_PointerClass);
    rb_global_variable(&rbffi_MemoryPointerClass);

    rb_define_alloc_func(rbffi_MemoryPointerClass, memptr_allocate);
    rb_define_method(rbffi_MemoryPointerClass, "initialize", memptr_initialize, -1);
    rb_define_method(rbffi_MemoryPointerClass, "inspect", memptr_inspect, 0);
    rb_define_method(rbffi_MemoryPointerClass, "autorelease=", memptr_autorelease, 1);
    rb_define_method(rbffi_MemoryPointerClass, "free", memptr_free, 0);
    rb_define_method(rbffi_MemoryPointerClass, "type_size", memptr_type_size, 0);
    rb_define_method(rbffi_MemoryPointerClass, "[]", memptr_aref, 1);
    
    plus_id = rb_intern("+");
}
