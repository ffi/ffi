#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"
#include "Pointer.h"

typedef struct Pointer {
    AbstractMemory memory;
    VALUE parent;
} Pointer;

#define POINTER(obj) rbffi_AbstractMemory_Cast((obj), rbffi_PointerClass)

VALUE rbffi_PointerClass;
static void ptr_mark(Pointer* ptr);

VALUE
rbffi_Pointer_NewInstance(void* addr)
{
    Pointer* p;
    VALUE obj;

    if (addr == NULL) {
        return rbffi_NullPointerSingleton;
    }

    obj = Data_Make_Struct(rbffi_PointerClass, Pointer, NULL, -1, p);
    p->memory.address = addr;
    p->memory.size = LONG_MAX;
    p->memory.ops = &rbffi_AbstractMemoryOps;
    p->parent = Qnil;

    return obj;
}

static VALUE
ptr_allocate(VALUE klass)
{
    Pointer* p;
    VALUE obj;

    obj = Data_Make_Struct(rbffi_PointerClass, Pointer, NULL, -1, p);
    p->parent = Qnil;
    p->memory.ops = &rbffi_AbstractMemoryOps;

    return obj;
}

static VALUE
ptr_initialize(int argc, VALUE* argv, VALUE self)
{
    Pointer* p;
    VALUE type, address;

    Data_Get_Struct(self, Pointer, p);
    p->memory.size = LONG_MAX;

    switch (rb_scan_args(argc, argv, "11", &type, &address)) {
        case 1:
            p->memory.address = (void*)(uintptr_t) NUM2LL(type);
            // FIXME set type_size to 1
            break;
        case 2:
            p->memory.address = (void*)(uintptr_t) NUM2LL(address);
            // FIXME set type_size to rbffi_type_size(type)
            break;
        default:
            rb_raise(rb_eArgError, "Invalid arguments");
    }

    return self;
}


static VALUE
ptr_plus(VALUE self, VALUE offset)
{
    AbstractMemory* ptr;
    Pointer* p;
    VALUE retval;
    long off = NUM2LONG(offset);

    Data_Get_Struct(self, AbstractMemory, ptr);
    checkBounds(ptr, off, 1);

    retval = Data_Make_Struct(rbffi_PointerClass, Pointer, ptr_mark, -1, p);

    p->memory.address = ptr->address + off;
    p->memory.size = ptr->size == LONG_MAX ? LONG_MAX : ptr->size - off;
    p->memory.ops = &rbffi_AbstractMemoryOps;
    p->parent = self;

    return retval;
}

static VALUE
ptr_inspect(VALUE self)
{
    Pointer* ptr;
    char tmp[100];

    Data_Get_Struct(self, Pointer, ptr);
    snprintf(tmp, sizeof(tmp), "#<Native Pointer address=%p>", ptr->memory.address);

    return rb_str_new2(tmp);
}

static VALUE
ptr_null_p(VALUE self)
{
    Pointer* ptr;

    Data_Get_Struct(self, Pointer, ptr);

    return ptr->memory.address == NULL ? Qtrue : Qfalse;
}

static VALUE
ptr_equals(VALUE self, VALUE other)
{
    Pointer* ptr;
    
    Data_Get_Struct(self, Pointer, ptr);

    return ptr->memory.address == POINTER(other)->address ? Qtrue : Qfalse;
}

static VALUE
ptr_address(VALUE self)
{
    Pointer* ptr;
    
    Data_Get_Struct(self, Pointer, ptr);

    return ULL2NUM((uintptr_t) ptr->memory.address);
}

static void
ptr_mark(Pointer* ptr)
{
    rb_gc_mark(ptr->parent);
}

void
rbffi_Pointer_Init(VALUE moduleFFI)
{
    rbffi_PointerClass = rb_define_class_under(moduleFFI, "Pointer", rbffi_AbstractMemoryClass);
    rb_global_variable(&rbffi_PointerClass);

    rb_define_alloc_func(rbffi_PointerClass, ptr_allocate);
    rb_define_method(rbffi_PointerClass, "initialize", ptr_initialize, -1);
    rb_define_method(rbffi_PointerClass, "inspect", ptr_inspect, 0);
    rb_define_method(rbffi_PointerClass, "+", ptr_plus, 1);
    rb_define_method(rbffi_PointerClass, "null?", ptr_null_p, 0);
    rb_define_method(rbffi_PointerClass, "address", ptr_address, 0);
    rb_define_alias(rbffi_PointerClass, "to_i", "address");
    rb_define_method(rbffi_PointerClass, "==", ptr_equals, 1);
}
