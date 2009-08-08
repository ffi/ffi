/*
 * Copyright (c) 2008, 2009, Wayne Meissner
 * Copyright (c) 2009, Luc Heinrich <luc@honk-honk.com>
 *
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

#include <sys/types.h>

#include "Function.h"
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>
#include <ruby.h>
#include "rbffi.h"
#include "compat.h"
#include "AbstractMemory.h"
#include "Pointer.h"
#include "MemoryPointer.h"
#include "Function.h"
#include "Types.h"
#include "Struct.h"
#include "StructByValue.h"
#include "ArrayType.h"

#define FFI_ALIGN(v, a)  (((((size_t) (v))-1) | ((a)-1))+1)

typedef struct StructLayoutBuilder {
    VALUE rbFieldNames;
    VALUE rbFieldMap;
    unsigned int size;
    unsigned int alignment;
    bool isUnion;
} StructLayoutBuilder;

typedef struct InlineArray_ {
    VALUE rbMemory;
    VALUE rbField;

    AbstractMemory* memory;
    StructField* field;
    MemoryOp *op;
    Type* componentType;
} InlineArray;


static void struct_mark(Struct *);
static void struct_layout_mark(StructLayout *);
static void struct_layout_free(StructLayout *);
static void struct_field_mark(StructField* );
static void struct_layout_builder_mark(StructLayoutBuilder *);
static void struct_layout_builder_free(StructLayoutBuilder *);


static void inline_array_mark(InlineArray *);

static inline MemoryOp* ptr_get_op(AbstractMemory* ptr, Type* type);
static inline int align(int offset, int align);

VALUE rbffi_StructClass = Qnil;
VALUE rbffi_StructLayoutClass = Qnil;
static VALUE StructFieldClass = Qnil, StructLayoutBuilderClass = Qnil;
static VALUE FunctionFieldClass = Qnil, ArrayFieldClass = Qnil, InlineStructFieldClass = Qnil;
static VALUE InlineArrayClass = Qnil;
static ID id_pointer_ivar = 0, id_layout_ivar = 0;
static ID id_get = 0, id_put = 0, id_to_ptr = 0, id_to_s = 0, id_layout = 0;

static VALUE
struct_field_allocate(VALUE klass)
{
    StructField* field;
    VALUE obj;
    
    obj = Data_Make_Struct(klass, StructField, struct_field_mark, -1, field);
    field->rbType = Qnil;
    field->rbName = Qnil;

    return obj;
}

static void
struct_field_mark(StructField* f)
{
    rb_gc_mark(f->rbType);
    rb_gc_mark(f->rbName);
}

static VALUE
struct_field_initialize(int argc, VALUE* argv, VALUE self)
{
    VALUE rbOffset = Qnil, rbName = Qnil, rbType = Qnil;
    StructField* field;
    int nargs;

    Data_Get_Struct(self, StructField, field);

    nargs = rb_scan_args(argc, argv, "3", &rbName, &rbOffset, &rbType);

    if (TYPE(rbName) != T_SYMBOL && TYPE(rbName) != T_STRING) {
        rb_raise(rb_eTypeError, "wrong argument type %s (expected Symbol/String)",
                rb_obj_classname(rbName));
    }

    Check_Type(rbOffset, T_FIXNUM);
    
    if (!rb_obj_is_kind_of(rbType, rbffi_TypeClass)) {
        rb_raise(rb_eTypeError, "wrong argument type %s (expected FFI::Type)",
                rb_obj_classname(rbType));
    }

    field->offset = NUM2UINT(rbOffset);
    field->rbName = (TYPE(rbName) == T_SYMBOL) ? rbName : rb_str_intern(rbName);
    field->rbType = rbType;
    Data_Get_Struct(field->rbType, Type, field->type);

    return self;
}

static VALUE
struct_field_offset(VALUE self)
{
    StructField* field;
    Data_Get_Struct(self, StructField, field);
    return UINT2NUM(field->offset);
}

static VALUE
struct_field_size(VALUE self)
{
    StructField* field;
    Data_Get_Struct(self, StructField, field);
    return UINT2NUM(field->type->ffiType->size);
}

static VALUE
struct_field_alignment(VALUE self)
{
    StructField* field;
    Data_Get_Struct(self, StructField, field);
    return UINT2NUM(field->type->ffiType->alignment);
}

static VALUE
struct_field_type(VALUE self)
{
    StructField* field;
    Data_Get_Struct(self, StructField, field);
    return field->rbType;
}

static VALUE
struct_field_name(VALUE self)
{
    StructField* field;
    Data_Get_Struct(self, StructField, field);
    return field->rbName;
}

static VALUE
struct_field_get(VALUE self, VALUE pointer)
{
    StructField* f;
    MemoryOp* op;
    AbstractMemory* memory = MEMORY(pointer);

    Data_Get_Struct(self, StructField, f);
    op = ptr_get_op(memory, f->type);
    if (op == NULL) {
        VALUE name = rb_class_name(CLASS_OF(self));
        rb_raise(rb_eArgError, "get not supported for %s", StringValueCStr(name));
        return Qnil;
    }

    return (*op->get)(memory, f->offset);
}

static VALUE
struct_field_put(VALUE self, VALUE pointer, VALUE value)
{
    StructField* f;
    MemoryOp* op;
    AbstractMemory* memory = MEMORY(pointer);

    Data_Get_Struct(self, StructField, f);
    op = ptr_get_op(memory, f->type);
    if (op == NULL) {
        VALUE name = rb_class_name(CLASS_OF(self));
        rb_raise(rb_eArgError, "put not supported for %s", StringValueCStr(name));
        return self;
    }
    
    (*op->put)(memory, f->offset, value);

    return self;
}

static VALUE
function_field_get(VALUE self, VALUE pointer)
{
    StructField* f;
    MemoryOp* op;
    AbstractMemory* memory = MEMORY(pointer);
    
    Data_Get_Struct(self, StructField, f);
    op = memory->ops->pointer;
    if (op == NULL) {
        VALUE name = rb_class_name(CLASS_OF(self));
        rb_raise(rb_eArgError, "get not supported for %s", StringValueCStr(name));
        return Qnil;
    }

    return rbffi_Function_NewInstance(f->rbType, (*op->get)(memory, f->offset));
}

static VALUE
function_field_put(VALUE self, VALUE pointer, VALUE proc)
{
    StructField* f;
    MemoryOp* op;
    AbstractMemory* memory = MEMORY(pointer);
    VALUE value = Qnil;

    Data_Get_Struct(self, StructField, f);
    op = memory->ops->pointer;
    if (op == NULL) {
        VALUE name = rb_class_name(CLASS_OF(self));
        rb_raise(rb_eArgError, "put not supported for %s", StringValueCStr(name));
        return self;
    }

    if (NIL_P(proc) || rb_obj_is_kind_of(proc, rbffi_FunctionClass)) {
        value = proc;
    } else if (rb_obj_is_kind_of(proc, rb_cProc) || rb_respond_to(proc, rb_intern("call"))) {
        value = rbffi_Function_ForProc(f->rbType, proc);
    } else {
        rb_raise(rb_eTypeError, "wrong type (expected Proc or Function)");
    }

    (*op->put)(memory, f->offset, value);

    return self;
}

static VALUE
array_field_get(VALUE self, VALUE pointer)
{
    StructField* f;
    ArrayType* array;
    MemoryOp* op;
    AbstractMemory* memory = MEMORY(pointer);
    VALUE argv[2];
    
    Data_Get_Struct(self, StructField, f);
    Data_Get_Struct(f->rbType, ArrayType, array);

    op = ptr_get_op(memory, array->componentType);
    if (op == NULL) {
        VALUE name = rb_class_name(array->rbComponentType);
        rb_raise(rb_eArgError, "get not supported for %s", StringValueCStr(name));
        return Qnil;
    }

    argv[0] = pointer;
    argv[1] = self;

    return rb_class_new_instance(2, argv, InlineArrayClass);
}

static VALUE
inline_struct_field_get(VALUE self, VALUE pointer)
{
    StructField* f;
    StructByValue* sbv;
    VALUE rbPointer = Qnil, rbOffset = Qnil;

    Data_Get_Struct(self, StructField, f);
    Data_Get_Struct(f->rbType, StructByValue, sbv);

    rbOffset = UINT2NUM(f->offset);
    rbPointer = rb_funcall2(pointer, rb_intern("+"), 1, &rbOffset);
    
    return rb_class_new_instance(1, &rbPointer, sbv->structClass);
}

static inline char*
memory_address(VALUE self)
{
    return ((AbstractMemory *)DATA_PTR((self)))->address;
}

static VALUE
struct_allocate(VALUE klass)
{
    Struct* s;
    VALUE obj = Data_Make_Struct(klass, Struct, struct_mark, -1, s);
    
    s->rbPointer = Qnil;
    s->rbLayout = Qnil;

    return obj;
}

static VALUE
struct_initialize(int argc, VALUE* argv, VALUE self)
{
    Struct* s;
    VALUE rbPointer = Qnil, rest = Qnil, klass = CLASS_OF(self);
    int nargs;

    Data_Get_Struct(self, Struct, s);
    
    nargs = rb_scan_args(argc, argv, "01*", &rbPointer, &rest);

    /* Call up into ruby code to adjust the layout */
    if (nargs > 1) {
        s->rbLayout = rb_funcall2(CLASS_OF(self), id_layout, RARRAY_LEN(rest), RARRAY_PTR(rest));
    } else if (rb_cvar_defined(klass, id_layout_ivar)) {
        s->rbLayout = rb_cvar_get(klass, id_layout_ivar);
    } else {
        rb_raise(rb_eRuntimeError, "No Struct layout configured");
    }

    if (!rb_obj_is_kind_of(s->rbLayout, rbffi_StructLayoutClass)) {
        rb_raise(rb_eRuntimeError, "Invalid Struct layout");
    }

    Data_Get_Struct(s->rbLayout, StructLayout, s->layout);
    
    if (rbPointer != Qnil) {
        s->pointer = MEMORY(rbPointer);
        s->rbPointer = rbPointer;
    } else {
        s->rbPointer = rbffi_MemoryPointer_NewInstance(s->layout->size, 1, true);
        s->pointer = (AbstractMemory *) DATA_PTR(s->rbPointer);
    }

    if (s->pointer->ops == NULL) {
        VALUE name = rb_class_name(CLASS_OF(s->rbPointer));
        rb_raise(rb_eRuntimeError, "No memory ops set for %s", StringValueCStr(name));
    }

    return self;
}

static void
struct_mark(Struct *s)
{
    rb_gc_mark(s->rbPointer);
    rb_gc_mark(s->rbLayout);
}

static VALUE
struct_field(Struct* s, VALUE fieldName)
{
    StructLayout* layout = s->layout;
    VALUE rbField;
    if (layout == NULL) {
        rb_raise(rb_eRuntimeError, "layout not set for Struct");
    }

    rbField = rb_hash_aref(layout->rbFieldMap, fieldName);
    if (rbField == Qnil) {
        VALUE str = rb_funcall2(fieldName, id_to_s, 0, NULL);
        rb_raise(rb_eArgError, "No such field '%s'", StringValuePtr(str));
    }

    return rbField;
}

static inline MemoryOp*
ptr_get_op(AbstractMemory* ptr, Type* type)
{
    if (ptr == NULL || ptr->ops == NULL || type == NULL) {
        return NULL;
    }
    switch (type->nativeType) {
        case NATIVE_INT8:
            return ptr->ops->int8;
        case NATIVE_UINT8:
            return ptr->ops->uint8;
        case NATIVE_INT16:
            return ptr->ops->int16;
        case NATIVE_UINT16:
            return ptr->ops->uint16;
        case NATIVE_INT32:
            return ptr->ops->int32;
        case NATIVE_UINT32:
            return ptr->ops->uint32;
        case NATIVE_INT64:
            return ptr->ops->int64;
        case NATIVE_UINT64:
            return ptr->ops->uint64;
        case NATIVE_FLOAT32:
            return ptr->ops->float32;
        case NATIVE_FLOAT64:
            return ptr->ops->float64;
        case NATIVE_POINTER:
            return ptr->ops->pointer;
        case NATIVE_STRING:
            return ptr->ops->strptr;
        default:
            return NULL;
    }
}

static VALUE
struct_aref(VALUE self, VALUE fieldName)
{
    Struct* s;
    VALUE rbField;
    StructField* f;
    MemoryOp* op;

    Data_Get_Struct(self, Struct, s);
    rbField = struct_field(s, fieldName);
    f = (StructField *) DATA_PTR(rbField);

    op = ptr_get_op(s->pointer, f->type);
    if (op != NULL) {
        return (*op->get)(s->pointer, f->offset);
    }
    
    /* call up to the ruby code to fetch the value */
    return rb_funcall2(rbField, id_get, 1, &s->rbPointer);
}

static VALUE
struct_aset(VALUE self, VALUE fieldName, VALUE value)
{
    Struct* s;
    VALUE rbField;
    StructField* f;
    MemoryOp* op;
    VALUE argv[2];

    Data_Get_Struct(self, Struct, s);
    rbField = struct_field(s, fieldName);
    f = (StructField *) DATA_PTR(rbField);

    op = ptr_get_op(s->pointer, f->type);
    if (op != NULL) {
        (*op->put)(s->pointer, f->offset, value);
        return self;
    }
    
    /* call up to the ruby code to set the value */
    argv[0] = s->rbPointer;
    argv[1] = value;
    rb_funcall2(rbField, id_put, 2, argv);
    
    return self;
}

static VALUE
struct_set_pointer(VALUE self, VALUE pointer)
{
    Struct* s;

    if (!rb_obj_is_kind_of(pointer, rbffi_AbstractMemoryClass)) {
        rb_raise(rb_eArgError, "Invalid pointer");
    }

    Data_Get_Struct(self, Struct, s);
    s->pointer = MEMORY(pointer);
    s->rbPointer = pointer;
    rb_ivar_set(self, id_pointer_ivar, pointer);

    return self;
}

static VALUE
struct_get_pointer(VALUE self)
{
    Struct* s;

    Data_Get_Struct(self, Struct, s);

    return s->rbPointer;
}

static VALUE
struct_set_layout(VALUE self, VALUE layout)
{
    Struct* s;
    Data_Get_Struct(self, Struct, s);

    if (!rb_obj_is_kind_of(layout, rbffi_StructLayoutClass)) {
        rb_raise(rb_eArgError, "Invalid Struct layout");
    }

    Data_Get_Struct(layout, StructLayout, s->layout);
    rb_ivar_set(self, id_layout_ivar, layout);

    return self;
}

static VALUE
struct_get_layout(VALUE self)
{
    Struct* s;

    Data_Get_Struct(self, Struct, s);

    return s->rbLayout;
}

static VALUE
struct_layout_allocate(VALUE klass)
{
    StructLayout* layout;
    VALUE obj;
    
    obj = Data_Make_Struct(klass, StructLayout, struct_layout_mark, struct_layout_free, layout);
    layout->rbFieldMap = Qnil;
    layout->rbFieldNames = Qnil;
    layout->rbFields = Qnil;
    layout->base.ffiType = xcalloc(1, sizeof(*layout->base.ffiType));
    layout->base.ffiType->size = 0;
    layout->base.ffiType->alignment = 0;
    layout->base.ffiType->type = FFI_TYPE_STRUCT;

    return obj;
}

static VALUE
struct_layout_initialize(VALUE self, VALUE field_names, VALUE fields, VALUE size, VALUE align)
{
    StructLayout* layout;
    ffi_type* ltype;
    int i;

    Data_Get_Struct(self, StructLayout, layout);
    layout->rbFieldMap = rb_hash_new();
    layout->rbFieldNames = rb_ary_dup(field_names);
    layout->size = NUM2INT(size);
    layout->align = NUM2INT(align);
    layout->fieldCount = RARRAY_LEN(field_names);
    layout->fields = xcalloc(layout->fieldCount, sizeof(StructField *));
    layout->ffiTypes = xcalloc(layout->fieldCount + 1, sizeof(ffi_type *));
    layout->rbTypes = ALLOC_N(VALUE, layout->fieldCount);
    layout->rbFields = rb_ary_new2(layout->fieldCount);
    layout->base.ffiType->elements = layout->ffiTypes;
    layout->base.ffiType->size = 0;
    layout->base.ffiType->alignment = 1;

    rb_iv_set(self, "@field_names", layout->rbFieldNames);
    rb_iv_set(self, "@fields", layout->rbFieldMap);
    rb_iv_set(self, "@size", size);
    rb_iv_set(self, "@align", align);

    ltype = layout->base.ffiType;
    for (i = 0; i < layout->fieldCount; ++i) {
        VALUE rbName = rb_ary_entry(field_names, i);
        VALUE rbField = rb_hash_aref(fields, rbName);
        StructField* field;
        ffi_type* ftype;
        

        if (!rb_obj_is_kind_of(rbField, StructFieldClass)) {
            rb_raise(rb_eTypeError, "wrong type for field %d.", i);
        }

        rb_hash_aset(layout->rbFieldMap, rbName, rbField);
        Data_Get_Struct(rbField, StructField, field = layout->fields[i]);
        

        if (field->type == NULL || field->type->ffiType == NULL) {
            rb_raise(rb_eRuntimeError, "type of field %d not supported", i);
        }
        ftype = field->type->ffiType;
        if (ftype->size == 0) {
            rb_raise(rb_eTypeError, "type of field %d  has zero size", i);
        }
        layout->ffiTypes[i] = ftype;
        layout->rbTypes[i] = field->rbType;
        rb_ary_push(layout->rbFields, rbField);
        ltype->size = MAX(ltype->size, field->offset + ftype->size);
        ltype->alignment = MAX(ltype->alignment, ftype->alignment);
    }

    if (ltype->size == 0) {
        rb_raise(rb_eRuntimeError, "Struct size is zero");
    }

    // Include tail padding
    ltype->size = FFI_ALIGN(ltype->size, ltype->alignment);

    return self;
}

static VALUE
struct_layout_aref(VALUE self, VALUE field)
{
    StructLayout* layout;

    Data_Get_Struct(self, StructLayout, layout);

    return rb_hash_aref(layout->rbFieldMap, field);
}

static VALUE
struct_layout_fields(VALUE self, VALUE field)
{
    StructLayout* layout;

    Data_Get_Struct(self, StructLayout, layout);

    return rb_ary_dup(layout->rbFields);
}


static void
struct_layout_mark(StructLayout *layout)
{
    rb_gc_mark(layout->rbFieldMap);
    rb_gc_mark(layout->rbFieldNames);
    rb_gc_mark(layout->rbFields);
    if (layout->rbTypes != NULL) {
        rb_gc_mark_locations(&layout->rbTypes[0], &layout->rbTypes[layout->fieldCount]);
    }
}

static void
struct_layout_free(StructLayout *layout)
{
    xfree(layout->ffiTypes);
    xfree(layout->base.ffiType);
    xfree(layout->fields);
    xfree(layout);
}

static VALUE
struct_layout_builder_allocate(VALUE klass)
{
    StructLayoutBuilder* builder;
    VALUE obj;

    obj = Data_Make_Struct(klass, StructLayoutBuilder, struct_layout_builder_mark, struct_layout_builder_free, builder);

    builder->size = 0;
    builder->alignment = 1;
    builder->isUnion = false;
    builder->rbFieldNames = rb_ary_new();
    builder->rbFieldMap = rb_hash_new();

    return obj;
}

static void
struct_layout_builder_mark(StructLayoutBuilder* builder)
{
    rb_gc_mark(builder->rbFieldNames);
    rb_gc_mark(builder->rbFieldMap);
}

static void
struct_layout_builder_free(StructLayoutBuilder* builder)
{
    xfree(builder);
}

static VALUE
struct_layout_builder_initialize(VALUE self)
{
    StructLayoutBuilder* builder;

    Data_Get_Struct(self, StructLayoutBuilder, builder);

    return self;
}

static VALUE
struct_layout_builder_get_size(VALUE self)
{
    StructLayoutBuilder* builder;

    Data_Get_Struct(self, StructLayoutBuilder, builder);

    return UINT2NUM(builder->size);
}

static VALUE
struct_layout_builder_set_size(VALUE self, VALUE rbSize)
{
    StructLayoutBuilder* builder;
    unsigned int size = NUM2UINT(rbSize);

    Data_Get_Struct(self, StructLayoutBuilder, builder);
    builder->size = MAX(size, builder->size);

    return UINT2NUM(builder->size);
}

static VALUE
struct_layout_builder_get_alignment(VALUE self)
{
    StructLayoutBuilder* builder;

    Data_Get_Struct(self, StructLayoutBuilder, builder);

    return UINT2NUM(builder->alignment);
}

static VALUE
struct_layout_builder_set_alignment(VALUE self, VALUE rbAlign)
{
    StructLayoutBuilder* builder;
    unsigned int align = NUM2UINT(rbAlign);

    Data_Get_Struct(self, StructLayoutBuilder, builder);
    builder->size = MAX(align, builder->alignment);

    return UINT2NUM(builder->alignment);
}

static VALUE
struct_layout_builder_set_union(VALUE self, VALUE rbUnion)
{
    StructLayoutBuilder* builder;


    Data_Get_Struct(self, StructLayoutBuilder, builder);
    builder->isUnion = RTEST(rbUnion);

    return rbUnion;
}

static VALUE
struct_layout_builder_union_p(VALUE self)
{
    StructLayoutBuilder* builder;


    Data_Get_Struct(self, StructLayoutBuilder, builder);


    return builder->isUnion ? Qtrue : Qfalse;
}

static void
store_field(StructLayoutBuilder* builder, VALUE rbName, VALUE rbField, 
    unsigned int offset, unsigned int size, unsigned int alignment)
{
    rb_ary_push(builder->rbFieldNames, rbName);
    rb_hash_aset(builder->rbFieldMap, rbName, rbField);

    builder->alignment = MAX(builder->alignment, alignment);

    if (builder->isUnion) {
        builder->size = MAX(builder->size, size);
    } else {
        builder->size = MAX(builder->size, offset + size);
    }
}

static int
calculate_offset(StructLayoutBuilder* builder, int alignment, VALUE rbOffset)
{
    if (rbOffset != Qnil) {
        return NUM2UINT(rbOffset);
    } else {
        return builder->isUnion ? 0 : align(builder->size, alignment);
    }
}

static VALUE
struct_layout_builder_add_field(int argc, VALUE* argv, VALUE self)
{
    StructLayoutBuilder* builder;
    VALUE rbName = Qnil, rbType = Qnil, rbOffset = Qnil, rbField = Qnil;
    unsigned int size, alignment, offset;
    int nargs;

    nargs = rb_scan_args(argc, argv, "21", &rbName, &rbType, &rbOffset);
    
    Data_Get_Struct(self, StructLayoutBuilder, builder);

    alignment = NUM2UINT(rb_funcall2(rbType, rb_intern("alignment"), 0, NULL));
    size = NUM2UINT(rb_funcall2(rbType, rb_intern("size"), 0, NULL));

    offset = calculate_offset(builder, alignment, rbOffset);

    //
    // If a primitive type was passed in as the type arg, try and convert
    //
    if (!rb_obj_is_kind_of(rbType, StructFieldClass)) {
        VALUE fargv[3], rbFieldClass;
        fargv[0] = rbName;
        fargv[1] = UINT2NUM(offset);
        fargv[2] = rbType;
        if (rb_obj_is_kind_of(rbType, rbffi_FunctionTypeClass)) {
            rbFieldClass = FunctionFieldClass;
        } else if (rb_obj_is_kind_of(rbType, rbffi_StructByValueClass)) {
            rbFieldClass = InlineStructFieldClass;
        } else if (rb_obj_is_kind_of(rbType, rbffi_ArrayTypeClass)) {
            rbFieldClass = ArrayFieldClass;
        } else {
            rbFieldClass = StructFieldClass;
        }

        rbField = rb_class_new_instance(3, fargv, rbFieldClass);
    } else {
        rbField = rbType;
    }

    store_field(builder, rbName, rbField, offset, size, alignment);
    
    return self;
}

static VALUE
struct_layout_builder_add_struct(int argc, VALUE* argv, VALUE self)
{
    StructLayoutBuilder* builder;
    VALUE rbName = Qnil, rbType = Qnil, rbOffset = Qnil, rbField = Qnil, rbStructClass = Qnil;
    VALUE fargv[3];
    unsigned int size, alignment, offset;
    int nargs;

    nargs = rb_scan_args(argc, argv, "21", &rbName, &rbStructClass, &rbOffset);

    if (!rb_obj_is_instance_of(rbStructClass, rb_cClass) || !rb_class_inherited(rbStructClass, rbffi_StructClass)) {
        rb_raise(rb_eTypeError, "wrong argument type.  Expected subclass of FFI::Struct");
    }

    rbType = rb_class_new_instance(1, &rbStructClass, rbffi_StructByValueClass);

    alignment = NUM2UINT(rb_funcall2(rbType, rb_intern("alignment"), 0, NULL));
    size = NUM2UINT(rb_funcall2(rbType, rb_intern("size"), 0, NULL));

    Data_Get_Struct(self, StructLayoutBuilder, builder);

    offset = calculate_offset(builder, alignment, rbOffset);

    fargv[0] = rbName;
    fargv[1] = UINT2NUM(offset);
    fargv[2] = rbType;
    rbField = rb_class_new_instance(3, fargv, InlineStructFieldClass);
    store_field(builder, rbName, rbField, offset, size, alignment);

    return self;
}

static VALUE
struct_layout_builder_add_array(int argc, VALUE* argv, VALUE self)
{
    StructLayoutBuilder* builder;
    VALUE rbName = Qnil, rbType = Qnil, rbLength = Qnil, rbOffset = Qnil, rbField;
    VALUE fargv[3], aargv[2];
    unsigned int size, alignment, offset;
    int nargs;

    nargs = rb_scan_args(argc, argv, "31", &rbName, &rbType, &rbLength, &rbOffset);

    Data_Get_Struct(self, StructLayoutBuilder, builder);

    alignment = NUM2UINT(rb_funcall2(rbType, rb_intern("alignment"), 0, NULL));
    size = NUM2UINT(rb_funcall2(rbType, rb_intern("size"), 0, NULL)) * NUM2UINT(rbLength);

    offset = calculate_offset(builder, alignment, rbOffset);

    aargv[0] = rbType;
    aargv[1] = rbLength;
    fargv[0] = rbName;
    fargv[1] = UINT2NUM(offset);
    fargv[2] = rb_class_new_instance(2, aargv, rbffi_ArrayTypeClass);
    rbField = rb_class_new_instance(3, fargv, ArrayFieldClass);

    store_field(builder, rbName, rbField, offset, size, alignment);

    return self;
}

static inline int
align(int offset, int align)
{
    return align + ((offset - 1) & ~(align - 1));
}

static VALUE
struct_layout_builder_build(VALUE self)
{
    StructLayoutBuilder* builder;
    VALUE argv[4];

    Data_Get_Struct(self, StructLayoutBuilder, builder);

    argv[0] = builder->rbFieldNames;
    argv[1] = builder->rbFieldMap;
    argv[2] = UINT2NUM(align(builder->size, builder->alignment)); // tail padding
    argv[3] = UINT2NUM(builder->alignment);

    return rb_class_new_instance(4, argv, rbffi_StructLayoutClass);
}

static VALUE
inline_array_allocate(VALUE klass)
{
    InlineArray* array;
    VALUE obj;

    obj = Data_Make_Struct(klass, InlineArray, inline_array_mark, -1, array);
    array->rbField = Qnil;
    array->rbMemory = Qnil;

    return obj;
}

static void
inline_array_mark(InlineArray* array)
{
    rb_gc_mark(array->rbField);
    rb_gc_mark(array->rbMemory);
}

static VALUE
inline_array_initialize(VALUE self, VALUE rbMemory, VALUE rbField)
{
    InlineArray* array;
    ArrayType* arrayType;

    Data_Get_Struct(self, InlineArray, array);
    array->rbMemory = rbMemory;
    array->rbField = rbField;

    Data_Get_Struct(rbMemory, AbstractMemory, array->memory);
    Data_Get_Struct(rbField, StructField, array->field);
    Data_Get_Struct(array->field->rbType, ArrayType, arrayType);
    Data_Get_Struct(arrayType->rbComponentType, Type, array->componentType);
    
    array->op = ptr_get_op(array->memory, array->componentType);
    if (array->op == NULL) {
        rb_raise(rb_eRuntimeError, "invalid memory ops");
    }

    return self;
}

static VALUE
inline_array_size(VALUE self)
{
    InlineArray* array;

    Data_Get_Struct(self, InlineArray, array);

    return UINT2NUM(array->field->type->ffiType->size);
}

static int
inline_array_offset(InlineArray* array, unsigned int index)
{
    return array->field->offset + (index * array->componentType->ffiType->size);
}

static VALUE
inline_array_aref(VALUE self, VALUE rbIndex)
{
    InlineArray* array;

    Data_Get_Struct(self, InlineArray, array);

    return array->op->get(array->memory, inline_array_offset(array, NUM2UINT(rbIndex)));
}

static VALUE
inline_array_aset(VALUE self, VALUE rbIndex, VALUE rbValue)
{
    InlineArray* array;

    Data_Get_Struct(self, InlineArray, array);

    array->op->put(array->memory, inline_array_offset(array, NUM2UINT(rbIndex)),
        rbValue);

    return rbValue;
}

static VALUE
inline_array_each(VALUE self)
{
    InlineArray* array;
    ArrayType* arrayType;
    
    int i;

    Data_Get_Struct(self, InlineArray, array);
    Data_Get_Struct(array->field->rbType, ArrayType, arrayType);

    for (i = 0; i < arrayType->length; ++i) {
    int offset = inline_array_offset(array, i);
        rb_yield(array->op->get(array->memory, offset));
    }

    return self;
}

static VALUE
inline_array_to_a(VALUE self)
{
    InlineArray* array;
    ArrayType* arrayType;
    VALUE obj;
    int i;

    Data_Get_Struct(self, InlineArray, array);
    Data_Get_Struct(array->field->rbType, ArrayType, arrayType);
    obj = rb_ary_new2(arrayType->length);

    
    for (i = 0; i < arrayType->length; ++i) {
        int offset = inline_array_offset(array, i);
        rb_ary_push(obj, array->op->get(array->memory, offset));
    }

    return obj;
}

static VALUE
inline_array_to_ptr(VALUE self)
{
    InlineArray* array;
    VALUE rbOffset;

    Data_Get_Struct(self, InlineArray, array);

    rbOffset = UINT2NUM(array->field->offset);
    return rb_funcall2(array->rbMemory, rb_intern("+"), 1, &rbOffset);
}


void
rbffi_Struct_Init(VALUE moduleFFI)
{
    VALUE StructClass;
    rbffi_StructClass = StructClass = rb_define_class_under(moduleFFI, "Struct", rb_cObject);
    rb_global_variable(&rbffi_StructClass);

    rbffi_StructLayoutClass = rb_define_class_under(moduleFFI, "StructLayout", rbffi_TypeClass);
    rb_global_variable(&rbffi_StructLayoutClass);

    StructLayoutBuilderClass = rb_define_class_under(moduleFFI, "StructLayoutBuilder", rb_cObject);
    rb_global_variable(&StructLayoutBuilderClass);

    StructFieldClass = rb_define_class_under(rbffi_StructLayoutClass, "Field", rb_cObject);
    rb_global_variable(&StructFieldClass);

    FunctionFieldClass = rb_define_class_under(rbffi_StructLayoutClass, "FunctionField", StructFieldClass);
    rb_global_variable(&FunctionFieldClass);

    InlineStructFieldClass = rb_define_class_under(rbffi_StructLayoutClass, "InlineStructField", StructFieldClass);
    rb_global_variable(&InlineStructFieldClass);

    ArrayFieldClass = rb_define_class_under(rbffi_StructLayoutClass, "InlineArrayField", StructFieldClass);
    rb_global_variable(&ArrayFieldClass);

    
    InlineArrayClass = rb_define_class_under(rbffi_StructLayoutClass, "InlineArray", rb_cObject);
    rb_global_variable(&InlineArrayClass);



    rb_define_alloc_func(StructClass, struct_allocate);
    rb_define_method(StructClass, "initialize", struct_initialize, -1);
    
    rb_define_alias(rb_singleton_class(StructClass), "alloc_in", "new");
    rb_define_alias(rb_singleton_class(StructClass), "alloc_out", "new");
    rb_define_alias(rb_singleton_class(StructClass), "alloc_inout", "new");
    rb_define_alias(rb_singleton_class(StructClass), "new_in", "new");
    rb_define_alias(rb_singleton_class(StructClass), "new_out", "new");
    rb_define_alias(rb_singleton_class(StructClass), "new_inout", "new");

    rb_define_method(StructClass, "pointer", struct_get_pointer, 0);
    rb_define_private_method(StructClass, "pointer=", struct_set_pointer, 1);

    rb_define_method(StructClass, "layout", struct_get_layout, 0);
    rb_define_private_method(StructClass, "layout=", struct_set_layout, 1);

    rb_define_method(StructClass, "[]", struct_aref, 1);
    rb_define_method(StructClass, "[]=", struct_aset, 2);
    
    rb_define_alloc_func(StructFieldClass, struct_field_allocate);
    rb_define_method(StructFieldClass, "initialize", struct_field_initialize, -1);
    rb_define_method(StructFieldClass, "offset", struct_field_offset, 0);
    rb_define_method(StructFieldClass, "size", struct_field_size, 0);
    rb_define_method(StructFieldClass, "alignment", struct_field_alignment, 0);
    rb_define_method(StructFieldClass, "name", struct_field_name, 0);
    rb_define_method(StructFieldClass, "type", struct_field_type, 0);
    rb_define_method(StructFieldClass, "put", struct_field_put, 2);
    rb_define_method(StructFieldClass, "get", struct_field_get, 1);

    rb_define_method(FunctionFieldClass, "put", function_field_put, 2);
    rb_define_method(FunctionFieldClass, "get", function_field_get, 1);

    rb_define_method(ArrayFieldClass, "get", array_field_get, 1);
    rb_define_method(InlineStructFieldClass, "get", inline_struct_field_get, 1);

    
    rb_define_alloc_func(rbffi_StructLayoutClass, struct_layout_allocate);
    rb_define_method(rbffi_StructLayoutClass, "initialize", struct_layout_initialize, 4);
    rb_define_method(rbffi_StructLayoutClass, "[]", struct_layout_aref, 1);
    rb_define_method(rbffi_StructLayoutClass, "fields", struct_layout_fields, 0);

    rb_define_alloc_func(StructLayoutBuilderClass, struct_layout_builder_allocate);
    rb_define_method(StructLayoutBuilderClass, "initialize", struct_layout_builder_initialize, 0);
    rb_define_method(StructLayoutBuilderClass, "build", struct_layout_builder_build, 0);

    rb_define_method(StructLayoutBuilderClass, "alignment", struct_layout_builder_get_alignment, 0);
    rb_define_method(StructLayoutBuilderClass, "alignment=", struct_layout_builder_set_alignment, 1);
    rb_define_method(StructLayoutBuilderClass, "size", struct_layout_builder_get_size, 0);
    rb_define_method(StructLayoutBuilderClass, "size=", struct_layout_builder_set_size, 1);
    rb_define_method(StructLayoutBuilderClass, "union=", struct_layout_builder_set_union, 1);
    rb_define_method(StructLayoutBuilderClass, "union?", struct_layout_builder_union_p, 0);
    rb_define_method(StructLayoutBuilderClass, "add_field", struct_layout_builder_add_field, -1);
    rb_define_method(StructLayoutBuilderClass, "add_array", struct_layout_builder_add_array, -1);
    rb_define_method(StructLayoutBuilderClass, "add_struct", struct_layout_builder_add_struct, -1);

    rb_include_module(InlineArrayClass, rb_mEnumerable);
    rb_define_alloc_func(InlineArrayClass, inline_array_allocate);
    rb_define_method(InlineArrayClass, "initialize", inline_array_initialize, 2);
    rb_define_method(InlineArrayClass, "[]", inline_array_aref, 1);
    rb_define_method(InlineArrayClass, "[]=", inline_array_aset, 2);
    rb_define_method(InlineArrayClass, "each", inline_array_each, 0);
    rb_define_method(InlineArrayClass, "size", inline_array_size, 0);
    rb_define_method(InlineArrayClass, "to_a", inline_array_to_a, 0);
    rb_define_method(InlineArrayClass, "to_ptr", inline_array_to_ptr, 0);

    id_pointer_ivar = rb_intern("@pointer");
    id_layout_ivar = rb_intern("@layout");
    id_layout = rb_intern("layout");
    id_get = rb_intern("get");
    id_put = rb_intern("put");
    id_to_ptr = rb_intern("to_ptr");
    id_to_s = rb_intern("to_s");
}
