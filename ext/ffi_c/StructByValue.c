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

#include "Type.h"
#include "StructByValue.h"

static VALUE sbv_allocate(VALUE);
static VALUE sbv_initialize(VALUE, VALUE);
static void sbv_mark(StructByValue *);

VALUE rbffi_StructByValueClass = Qnil;

static VALUE
sbv_allocate(VALUE klass)
{
    StructByValue* sbv;

    VALUE obj = Data_Make_Struct(klass, StructByValue, sbv_mark, -1, sbv);

    sbv->structClass = Qnil;
    sbv->type.nativeType = NATIVE_STRUCT;


    return obj;
}

static VALUE
sbv_initialize(VALUE self, VALUE structClass)
{
    StructByValue* sbv;
    
    Data_Get_Struct(self, StructByValue, sbv);

    sbv->structClass = structClass;

    return self;
}

static void
sbv_mark(StructByValue *sbv)
{
    rb_gc_mark(sbv->structClass);
}

void
rbffi_StructByValue_Init(VALUE moduleFFI)
{
    rbffi_StructByValueClass = rb_define_class_under(moduleFFI, "StructByValue", rbffi_TypeClass);
    rb_global_variable(&rbffi_StructByValueClass);

    rb_define_alloc_func(rbffi_StructByValueClass, sbv_allocate);
    rb_define_method(rbffi_StructByValueClass, "initialize", sbv_initialize, 1);

}

