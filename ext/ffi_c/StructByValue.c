/*
 * Copyright (c) 2009, Wayne Meissner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * The name of the author or authors may not be used to endorse or promote
 *   products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <ruby.h>

#include <ffi.h>
#include "rbffi.h"
#include "compat.h"

#include "Type.h"
#include "StructByValue.h"
#include "Struct.h"

#define FFI_ALIGN(v, a)  (((((size_t) (v))-1) | ((a)-1))+1)

static VALUE sbv_allocate(VALUE);
static VALUE sbv_initialize(VALUE, VALUE);
static void sbv_mark(StructByValue *);
static void sbv_free(StructByValue *);

VALUE rbffi_StructByValueClass = Qnil;

static VALUE
sbv_allocate(VALUE klass)
{
    StructByValue* sbv;

    VALUE obj = Data_Make_Struct(klass, StructByValue, sbv_mark, sbv_free, sbv);

    sbv->structClass = Qnil;
    sbv->type.nativeType = NATIVE_STRUCT;

    sbv->type.ffiType = xcalloc(1, sizeof(*sbv->type.ffiType));
    sbv->type.ffiType->size = 0;
    sbv->type.ffiType->alignment = 0;
    sbv->type.ffiType->type = FFI_TYPE_STRUCT;

    return obj;
}

static VALUE
sbv_initialize(VALUE self, VALUE structClass)
{
    StructByValue* sbv;
    StructLayout* layout;
    VALUE rbLayout;
    int i;

    Data_Get_Struct(self, StructByValue, sbv);
    sbv->structClass = structClass;

    rbLayout = rb_cvar_get(structClass, rb_intern("@layout"));
    if (!rb_obj_is_instance_of(rbLayout, rbffi_StructLayoutClass)) {
        rb_raise(rb_eTypeError, "wrong type in @layout cvar (expected StructLayout)");
    }
    Data_Get_Struct(rbLayout, StructLayout, layout);

    sbv->fieldCount = layout->fieldCount;

    // ffiTypes is NULL terminated, so allocate 1 extra
    sbv->ffiTypes = xcalloc(layout->fieldCount + 1, sizeof(ffi_type *));
    sbv->rbFields = xcalloc(layout->fieldCount, sizeof(VALUE));
    sbv->type.ffiType->size = 0;
    sbv->type.ffiType->alignment = 0;

    for (i = 0; i < sbv->fieldCount; ++i) {
        StructField* f = layout->fields[i];
        ffi_type* ftype = f->type->ffiType;
        ffi_type* stype = sbv->type.ffiType;

        if (ftype == NULL) {
            rb_raise(rb_eRuntimeError, "type of field %d not supported for struct-by-value yet", i);
        }

        if (ftype->size == 0) {
            rb_raise(rb_eTypeError, "type of field %d  has zero size", i);
        }

        sbv->ffiTypes[i] = ftype;
        sbv->rbFields[i] = Qnil;
        stype->size = MAX(stype->size, f->offset + ftype->size);
        stype->alignment = MAX(stype->alignment, ftype->alignment);
    }

    if (sbv->type.ffiType->size == 0) {
        rb_raise(rb_eRuntimeError, "Struct size is zero");
    }

    // Include tail padding
    sbv->type.ffiType->size = FFI_ALIGN(sbv->type.ffiType->size, sbv->type.ffiType->alignment);
    sbv->type.ffiType->elements = sbv->ffiTypes;

    return self;
}

static void
sbv_mark(StructByValue *sbv)
{
    rb_gc_mark(sbv->structClass);
    if (sbv->rbFields != NULL) {
        rb_gc_mark_locations(&sbv->rbFields[0], &sbv->rbFields[sbv->fieldCount]);
    }
}

static void
sbv_free(StructByValue *sbv)
{
    xfree(sbv->type.ffiType);
    xfree(sbv->ffiTypes);
    xfree(sbv->rbFields);
    xfree(sbv);
}

void
rbffi_StructByValue_Init(VALUE moduleFFI)
{
    rbffi_StructByValueClass = rb_define_class_under(moduleFFI, "StructByValue", rbffi_TypeClass);
    rb_global_variable(&rbffi_StructByValueClass);

    rb_define_alloc_func(rbffi_StructByValueClass, sbv_allocate);
    rb_define_method(rbffi_StructByValueClass, "initialize", sbv_initialize, 1);

}

