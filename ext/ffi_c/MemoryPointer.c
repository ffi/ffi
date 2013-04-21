/*
 * Copyright (c) 2008, 2009, Wayne Meissner
 * Copyright (c) 2008, Luc Heinrich <luc@honk-honk.com>
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

#ifndef _MSC_VER
# include <stdbool.h>
# include <stdint.h>
#else
# include "win32/stdbool.h"
# include "win32/stdint.h"
#endif
#include <limits.h>
#include <ruby.h>
#include "rbffi.h"
#include "AbstractMemory.h"
#include "Pointer.h"
#include "MemoryPointer.h"


static VALUE memptr_allocate(VALUE klass);
static void memptr_release(Pointer* ptr);
static VALUE memptr_malloc(VALUE self, long size, long count, bool clear);
static VALUE memptr_free(VALUE self);

VALUE rbffi_MemoryPointerClass;

#define MEMPTR(obj) ((MemoryPointer *) rbffi_AbstractMemory_Cast(obj, rbffi_MemoryPointerClass))

VALUE
rbffi_MemoryPointer_NewInstance(long size, long count, bool clear)
{
    return memptr_malloc(memptr_allocate(rbffi_MemoryPointerClass), size, count, clear);
}

static VALUE
memptr_allocate(VALUE klass)
{
    Pointer* p;
    VALUE obj = Data_Make_Struct(klass, Pointer, NULL, memptr_release, p);
    p->rbParent = Qnil;
    p->memory.flags = MEM_RD | MEM_WR;

    return obj;
}

/*
 * call-seq: initialize(size, count=1, clear=true)
 * @param [Fixnum, Bignum, Symbol, FFI::Type] size size of a memory cell (in bytes, or type whom size will be used)
 * @param [Numeric] count number of cells in memory
 * @param [Boolean] clear set memory to all-zero if +true+
 * @return [self]
 * A new instance of FFI::MeoryPointer.
 */
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
    Pointer* p;
    unsigned long msize;

    Data_Get_Struct(self, Pointer, p);

    msize = size * count;

    p->storage = xmalloc(msize + 7);
    if (p->storage == NULL) {
        rb_raise(rb_eNoMemError, "Failed to allocate memory size=%ld bytes", msize);
        return Qnil;
    }
    p->autorelease = true;
    p->memory.typeSize = (int) size;
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
memptr_free(VALUE self)
{
    Pointer* ptr;

    Data_Get_Struct(self, Pointer, ptr);

    if (ptr->allocated) {
        if (ptr->storage != NULL) {
            xfree(ptr->storage);
            ptr->storage = NULL;
        }
        ptr->allocated = false;
    }

    return self;
}

static void
memptr_release(Pointer* ptr)
{
    if (ptr->autorelease && ptr->allocated && ptr->storage != NULL) {
        xfree(ptr->storage);
        ptr->storage = NULL;
    }
    xfree(ptr);
}

/*
 * call-seq: from_string(s)
 * @param [String] s string
 * @return [MemoryPointer]
 * Create a {MemoryPointer} with +s+ inside.
 */
static VALUE
memptr_s_from_string(VALUE klass, VALUE to_str)
{
    VALUE s = StringValue(to_str);
    VALUE args[] = { INT2FIX(1), LONG2NUM(RSTRING_LEN(s) + 1), Qfalse };
    VALUE obj = rb_class_new_instance(3, args, klass);
    rb_funcall(obj, rb_intern("put_string"), 2, INT2FIX(0), s);

    return obj;
}

void
rbffi_MemoryPointer_Init(VALUE moduleFFI)
{
    VALUE ffi_Pointer;

    ffi_Pointer = rbffi_PointerClass;

    /*
     * Document-class: FFI::MemoryPointer < FFI::Pointer
     * A MemoryPointer is a specific {Pointer}. It points to a memory composed of cells. All cells have the
     * same size.
     *
     * @example Create a new MemoryPointer
     *  mp = FFI::MemoryPointer.new(:long, 16)   # Create a pointer on a memory of 16 long ints.
     * @example Create a new MemoryPointer from a String
     *  mp1 = FFI::MemoryPointer.from_string("this is a string")
     *  # same as:
     *  mp2 = FFI::MemoryPointer.new(:char,16)
     *  mp2.put_string("this is a string")
     */
    rbffi_MemoryPointerClass = rb_define_class_under(moduleFFI, "MemoryPointer", ffi_Pointer);
    rb_global_variable(&rbffi_MemoryPointerClass);

    rb_define_alloc_func(rbffi_MemoryPointerClass, memptr_allocate);
    rb_define_method(rbffi_MemoryPointerClass, "initialize", memptr_initialize, -1);
    rb_define_singleton_method(rbffi_MemoryPointerClass, "from_string", memptr_s_from_string, 1);
}

