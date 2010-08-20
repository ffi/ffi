/*
 * Copyright (c) 2008, 2009, Wayne Meissner
 *
 * All rights reserved.
 *
 * This file is part of ruby-ffi.
 *
 * This code is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * version 3 for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with this work.  If not, see <http://www.gnu.org/licenses/>.
 */

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

VALUE rbffi_PointerClass = Qnil;
VALUE rbffi_NullPointerSingleton = Qnil;

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
    p->memory.flags = (addr == NULL) ? 0 : (MEM_RD | MEM_WR);
    p->memory.typeSize = 1;
    p->parent = Qnil;

    return obj;
}

static VALUE
ptr_allocate(VALUE klass)
{
    Pointer* p;
    VALUE obj;

    obj = Data_Make_Struct(klass, Pointer, ptr_mark, -1, p);
    p->parent = Qnil;
    p->memory.flags = MEM_RD | MEM_WR;

    return obj;
}

static VALUE
ptr_initialize(int argc, VALUE* argv, VALUE self)
{
    Pointer* p;
    VALUE rbType = Qnil, rbAddress = Qnil;
    int typeSize = 1;

    Data_Get_Struct(self, Pointer, p);

    switch (rb_scan_args(argc, argv, "11", &rbType, &rbAddress)) {
        case 1:
            rbAddress = rbType;
            typeSize = 1;
            break;
        case 2:
            typeSize = rbffi_type_size(rbType);
            break;
        default:
            rb_raise(rb_eArgError, "Invalid arguments");
    }

    switch (TYPE(rbAddress)) {
        case T_FIXNUM:
        case T_BIGNUM:
            p->memory.address = (void*) (uintptr_t) NUM2LL(rbAddress);
            p->memory.size = LONG_MAX;
            if (p->memory.address == NULL) {
                p->memory.flags = 0;
            }
            break;

        default:
            if (rb_obj_is_kind_of(rbAddress, rbffi_PointerClass)) {
                Pointer* orig;

                p->parent = rbAddress;
                Data_Get_Struct(rbAddress, Pointer, orig);
                p->memory = orig->memory;
            } else {
                rb_raise(rb_eTypeError, "wrong argument type, expected Integer or FFI::Pointer");
            }
            break;
    }

    p->memory.typeSize = typeSize;

    return self;
}


static VALUE
slice(VALUE self, long offset, long size)
{
    AbstractMemory* ptr;
    Pointer* p;
    VALUE retval;
    
    Data_Get_Struct(self, AbstractMemory, ptr);
    checkBounds(ptr, offset, 1);

    retval = Data_Make_Struct(rbffi_PointerClass, Pointer, ptr_mark, -1, p);

    p->memory.address = ptr->address + offset;
    p->memory.size = size;
    p->memory.flags = ptr->flags;
    p->memory.typeSize = ptr->typeSize;
    p->parent = self;

    return retval;
}

static VALUE
ptr_plus(VALUE self, VALUE offset)
{
    AbstractMemory* ptr;
    long off = NUM2LONG(offset);

    Data_Get_Struct(self, AbstractMemory, ptr);

    return slice(self, off, ptr->size == LONG_MAX ? LONG_MAX : ptr->size - off);
}

static VALUE
ptr_slice(VALUE self, VALUE rbOffset, VALUE rbLength)
{
    return slice(self, NUM2LONG(rbOffset), NUM2LONG(rbLength));
}

static VALUE
ptr_inspect(VALUE self)
{
    Pointer* ptr;
    
    Data_Get_Struct(self, Pointer, ptr);

    return ptr->memory.size != LONG_MAX
        ? rb_sprintf("#<%s address=%p size=%lu>", rb_obj_classname(self), ptr->memory.address, ptr->memory.size)
        : rb_sprintf("#<%s address=%p>", rb_obj_classname(self), ptr->memory.address);
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

#if BYTE_ORDER == LITTLE_ENDIAN
# define SWAPPED_ORDER BIG_ENDIAN
#else
# define SWAPPED_ORDER LITTLE_ENDIAN
#endif

static VALUE
ptr_order(int argc, VALUE* argv, VALUE self)
{
    Pointer* ptr;

    Data_Get_Struct(self, Pointer, ptr);
    if (argc == 0) {
        int order = (ptr->memory.flags & MEM_SWAP) == 0 ? BYTE_ORDER : SWAPPED_ORDER;
        return order == BIG_ENDIAN ? ID2SYM(rb_intern("big")) : ID2SYM(rb_intern("little"));
    } else {
        VALUE rbOrder = Qnil;
        int order = BYTE_ORDER;

        if (rb_scan_args(argc, argv, "1", &rbOrder) < 1) {
            rb_raise(rb_eArgError, "need byte order");
        }
        if (SYMBOL_P(rbOrder)) {
            ID id = SYM2ID(rbOrder);
            if (id == rb_intern("little")) {
                order = LITTLE_ENDIAN;

            } else if (id == rb_intern("big") || id == rb_intern("network")) {
                order = BIG_ENDIAN;
            }
        }
        if (order != BYTE_ORDER) {
            Pointer* p2;
            VALUE retval = slice(self, 0, ptr->memory.size);

            Data_Get_Struct(retval, Pointer, p2);
            p2->memory.flags |= MEM_SWAP;
            return retval;
        }

        return self;
    }
}

static void
ptr_mark(Pointer* ptr)
{
    rb_gc_mark(ptr->parent);
}

void
rbffi_Pointer_Init(VALUE moduleFFI)
{
    VALUE rbNullAddress = ULL2NUM(0);

    rbffi_PointerClass = rb_define_class_under(moduleFFI, "Pointer", rbffi_AbstractMemoryClass);
    rb_global_variable(&rbffi_PointerClass);

    rb_define_alloc_func(rbffi_PointerClass, ptr_allocate);
    rb_define_method(rbffi_PointerClass, "initialize", ptr_initialize, -1);
    rb_define_method(rbffi_PointerClass, "inspect", ptr_inspect, 0);
    rb_define_method(rbffi_PointerClass, "to_s", ptr_inspect, 0);
    rb_define_method(rbffi_PointerClass, "+", ptr_plus, 1);
    rb_define_method(rbffi_PointerClass, "slice", ptr_slice, 2);
    rb_define_method(rbffi_PointerClass, "null?", ptr_null_p, 0);
    rb_define_method(rbffi_PointerClass, "address", ptr_address, 0);
    rb_define_alias(rbffi_PointerClass, "to_i", "address");
    rb_define_method(rbffi_PointerClass, "==", ptr_equals, 1);
    rb_define_method(rbffi_PointerClass, "order", ptr_order, -1);

    rbffi_NullPointerSingleton = rb_class_new_instance(1, &rbNullAddress, rbffi_PointerClass);
    rb_define_const(rbffi_PointerClass, "NULL", rbffi_NullPointerSingleton);
}
