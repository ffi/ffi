#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>
#include <ruby.h>
#include "rbffi.h"
#include "compat.h"
#include "AbstractMemory.h"
#include "Pointer.h"
#include "MemoryPointer.h"
#include "Types.h"
#include "Struct.h"

typedef struct StructField {
    unsigned int type;
    unsigned int offset;
    unsigned int size;
    unsigned int align;
} StructField;

typedef struct StructLayout {
    VALUE rbFields;
    unsigned int fieldCount;
    int size;
    int align;
} StructLayout;

typedef struct StructLayoutBuilder {
    unsigned int offset;
} StructLayoutBuilder;

static void struct_mark(Struct *);
static void struct_layout_mark(StructLayout *);
static inline MemoryOp* ptr_get_op(AbstractMemory* ptr, int type);

VALUE rb_FFI_Struct_class = Qnil;
static VALUE classStruct = Qnil, classStructLayout = Qnil;
static VALUE classStructField = Qnil, classStructLayoutBuilder = Qnil;
static ID pointer_var_id = 0, layout_var_id = 0, SIZE_ID, ALIGN_ID, TYPE_ID;
static ID get_id = 0, put_id = 0, to_ptr = 0, to_s = 0, layout_id = 0;

static VALUE
struct_field_allocate(VALUE klass)
{
    StructField* field;
    return Data_Make_Struct(klass, StructField, NULL, -1, field);
}

static VALUE
struct_field_initialize(int argc, VALUE* argv, VALUE self)
{
    VALUE offset = Qnil, info = Qnil;
    StructField* field;
    int nargs;

    Data_Get_Struct(self, StructField, field);

    nargs = rb_scan_args(argc, argv, "11", &offset, &info);
    
    field->offset = NUM2UINT(offset);
    if (rb_const_defined(CLASS_OF(self), TYPE_ID)) {
        field->type = NUM2UINT(rb_const_get(CLASS_OF(self), TYPE_ID));
    } else {
        field->type = ~0;
    }

#ifdef notyet
    field->size = NUM2UINT(rb_const_get(klass, SIZE_ID));
    field->align = NUM2UINT(rb_const_get(klass, ALIGN_ID));
#endif
    rb_iv_set(self, "@off", offset);
    rb_iv_set(self, "@info", info);

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
        s->rbLayout = rb_funcall2(CLASS_OF(self), layout_id, RARRAY_LEN(rest), RARRAY_PTR(rest));
    } else if (rb_cvar_defined(klass, layout_var_id)) {
        s->rbLayout = rb_cvar_get(klass, layout_var_id);
    } else {
        rb_raise(rb_eRuntimeError, "No Struct layout configured");
    }

    if (!rb_obj_is_kind_of(s->rbLayout, classStructLayout)) {
        rb_raise(rb_eRuntimeError, "Invalid Struct layout");
    }

    Data_Get_Struct(s->rbLayout, StructLayout, s->layout);
    
    if (rbPointer != Qnil) {
        s->pointer = MEMORY(rbPointer);
        s->rbPointer = rbPointer;
    } else {
        s->rbPointer = rb_FFI_MemoryPointer_new(s->layout->size, 1, true);
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

    rbField = rb_hash_aref(layout->rbFields, fieldName);
    if (rbField == Qnil) {
        VALUE str = rb_funcall2(fieldName, to_s, 0, NULL);
        rb_raise(rb_eArgError, "No such field '%s'", StringValuePtr(str));
    }

    return rbField;
}

static inline MemoryOp*
ptr_get_op(AbstractMemory* ptr, int type)
{
    if (ptr == NULL || ptr->ops == NULL) {
        return NULL;
    }
    switch (type) {
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
struct_get_field(VALUE self, VALUE fieldName)
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
    return rb_funcall2(rbField, get_id, 1, &s->rbPointer);
}

static VALUE
struct_put_field(VALUE self, VALUE fieldName, VALUE value)
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
    rb_funcall2(rbField, put_id, 2, argv);
    
    return self;
}

static VALUE
struct_set_pointer(VALUE self, VALUE pointer)
{
    Struct* s;

    if (!rb_obj_is_kind_of(pointer, rb_FFI_AbstractMemory_class)) {
        rb_raise(rb_eArgError, "Invalid pointer");
    }

    Data_Get_Struct(self, Struct, s);
    s->pointer = MEMORY(pointer);
    s->rbPointer = pointer;
    rb_ivar_set(self, pointer_var_id, pointer);

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

    if (!rb_obj_is_kind_of(layout, classStructLayout)) {
        rb_raise(rb_eArgError, "Invalid Struct layout");
    }

    Data_Get_Struct(layout, StructLayout, s->layout);
    rb_ivar_set(self, layout_var_id, layout);

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
    
    obj = Data_Make_Struct(klass, StructLayout, struct_layout_mark, -1, layout);
    layout->rbFields = Qnil;

    return obj;
}

static VALUE
struct_layout_initialize(VALUE self, VALUE field_names, VALUE fields, VALUE size, VALUE align)
{
    StructLayout* layout;
    int i;

    Data_Get_Struct(self, StructLayout, layout);
    layout->rbFields = rb_hash_new();
    layout->size = NUM2INT(size);
    layout->align = NUM2INT(align);
    
    rb_iv_set(self, "@field_names", field_names);
    rb_iv_set(self, "@fields", fields);
    rb_iv_set(self, "@size", size);
    rb_iv_set(self, "@align", align);

    for (i = 0; i < RARRAY_LEN(field_names); ++i) {
        VALUE name = RARRAY_PTR(field_names)[i];
        VALUE field = rb_hash_aref(fields, name);
        if (TYPE(field) != T_DATA || !rb_obj_is_kind_of(field, classStructField)) {
            rb_raise(rb_eArgError, "Invalid field");
        }
        rb_hash_aset(layout->rbFields, name, field);
    }
    return self;
}

static VALUE
struct_layout_aref(VALUE self, VALUE field)
{
    StructLayout* layout;

    Data_Get_Struct(self, StructLayout, layout);

    return rb_hash_aref(layout->rbFields, field);
}


static void
struct_layout_mark(StructLayout *layout)
{
    rb_gc_mark(layout->rbFields);
}

void
rb_FFI_Struct_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    VALUE klass;
    rb_FFI_Struct_class = classStruct = rb_define_class_under(moduleFFI, "Struct", rb_cObject);
    rb_global_variable(&rb_FFI_Struct_class);
    rb_global_variable(&classStruct);
    classStructLayout = rb_define_class_under(moduleFFI, "StructLayout", rb_cObject);
    rb_global_variable(&classStructLayout);
    classStructLayoutBuilder = rb_define_class_under(moduleFFI, "StructLayoutBuilder", rb_cObject);
    rb_global_variable(&classStructLayoutBuilder);
    classStructField = rb_define_class_under(classStructLayoutBuilder, "Field", rb_cObject);
    rb_global_variable(&classStructField);

    rb_define_alloc_func(classStruct, struct_allocate);
    rb_define_method(classStruct, "initialize", struct_initialize, -1);
    
    rb_define_alias(rb_singleton_class(classStruct), "alloc_in", "new");
    rb_define_alias(rb_singleton_class(classStruct), "alloc_out", "new");
    rb_define_alias(rb_singleton_class(classStruct), "alloc_inout", "new");
    rb_define_alias(rb_singleton_class(classStruct), "new_in", "new");
    rb_define_alias(rb_singleton_class(classStruct), "new_out", "new");
    rb_define_alias(rb_singleton_class(classStruct), "new_inout", "new");

    rb_define_method(classStruct, "pointer", struct_get_pointer, 0);
    rb_define_private_method(classStruct, "pointer=", struct_set_pointer, 1);

    rb_define_method(classStruct, "layout", struct_get_layout, 0);
    rb_define_private_method(classStruct, "layout=", struct_set_layout, 1);

    rb_define_method(classStruct, "[]", struct_get_field, 1);
    rb_define_method(classStruct, "[]=", struct_put_field, 2);
    
    rb_define_alloc_func(classStructField, struct_field_allocate);
    rb_define_method(classStructField, "initialize", struct_field_initialize, -1);
    rb_define_method(classStructField, "offset", struct_field_offset, 0);
    rb_define_method(classStructField, "put", struct_field_put, 2);
    rb_define_method(classStructField, "get", struct_field_get, 1);

    rb_define_alloc_func(classStructLayout, struct_layout_allocate);
    rb_define_method(classStructLayout, "initialize", struct_layout_initialize, 4);
    rb_define_method(classStructLayout, "[]", struct_layout_aref, 1);

    pointer_var_id = rb_intern("@pointer");
    layout_var_id = rb_intern("@layout");
    layout_id = rb_intern("layout");
    get_id = rb_intern("get");
    put_id = rb_intern("put");
    to_ptr = rb_intern("to_ptr");
    to_s = rb_intern("to_s");
    SIZE_ID = rb_intern("SIZE");
    ALIGN_ID = rb_intern("ALIGN");
    TYPE_ID = rb_intern("TYPE");
#undef FIELD
#define FIELD(name, typeName, nativeType, T) do { \
    typedef struct { char c; T v; } s; \
        klass = rb_define_class_under(classStructLayoutBuilder, #name, classStructField); \
        rb_define_const(klass, "ALIGN", INT2NUM((sizeof(s) - sizeof(T)) * 8)); \
        rb_define_const(klass, "SIZE", INT2NUM(sizeof(T)* 8)); \
        rb_define_const(klass, "TYPE", INT2NUM(nativeType)); \
    } while(0)
    
    FIELD(Signed8, int8, NATIVE_INT8, char);
    FIELD(Unsigned8, uint8, NATIVE_UINT8, unsigned char);
    FIELD(Signed16, int16, NATIVE_INT16, short);
    FIELD(Unsigned16, uint16, NATIVE_UINT16, unsigned short);
    FIELD(Signed32, int32, NATIVE_INT32, int);
    FIELD(Unsigned32, uint32, NATIVE_UINT32, unsigned int);
    FIELD(Signed64, int64, NATIVE_INT64, long long);
    FIELD(Unsigned64, uint64, NATIVE_UINT64, unsigned long long);
    FIELD(FloatField, float32, NATIVE_FLOAT32, float);
    FIELD(DoubleField, float64, NATIVE_FLOAT64, double);
    FIELD(PointerField, pointer, NATIVE_POINTER, char *);
    FIELD(StringField, string, NATIVE_STRING, char *);
}
