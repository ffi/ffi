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

static void struct_field_mark(StructField *);
static void struct_field_free(StructField *);
static void struct_mark(Struct *);
static void struct_free(Struct *);
static void struct_layout_mark(StructLayout *);
static void struct_layout_free(StructLayout *);

VALUE rb_FFI_Struct_class = Qnil;
static VALUE classStruct = Qnil, classStructLayout = Qnil;
static VALUE classStructField = Qnil, classStructLayoutBuilder = Qnil;
static ID initializeID = 0, pointerID = 0, layoutID = 0, SIZE_ID, ALIGN_ID;
static ID getID = 0, putID = 0, to_ptr = 0;

#define FIELD_CAST(obj) ((StructField *)((TYPE(obj) == T_DATA && rb_obj_is_kind_of(obj, classStructField)) \
    ? DATA_PTR(obj) : (rb_raise(rb_eArgError, "StructField expected"), NULL)))

#define LAYOUT_CAST(obj) ((StructLayout *)((TYPE(obj) == T_DATA && rb_obj_is_kind_of(obj, classStructLayout)) \
    ? DATA_PTR(obj) : (rb_raise(rb_eArgError, "StructLayout expected"), NULL)))

static VALUE
struct_field_new(int argc, VALUE* argv, VALUE klass)
{
    VALUE offset = Qnil, info = Qnil;
    StructField* field;
    VALUE retval;
    int nargs;
    nargs = rb_scan_args(argc, argv, "11", &offset, &info);
    retval = Data_Make_Struct(klass, StructField, struct_field_mark, struct_field_free, field);
    field->offset = NUM2UINT(offset);
    if (rb_const_defined(klass, rb_intern("TYPE"))) {
        field->type = NUM2UINT(rb_const_get(klass, rb_intern("TYPE")));
    } else {
        field->type = ~0;
    }
#ifdef notyet
    field->size = NUM2UINT(rb_const_get(klass, rb_intern("SIZE")));
    field->align = NUM2UINT(rb_const_get(klass, rb_intern("ALIGN")));
#endif
    rb_iv_set(retval, "@off", offset);
    rb_iv_set(retval, "@info", info);
    return retval;
}

static VALUE
struct_field_offset(VALUE self)
{
    StructField* field;
    Data_Get_Struct(self, StructField, field);
    return UINT2NUM(field->offset);
}

static void
struct_field_mark(StructField *f)
{
}
static void
struct_field_free(StructField *f)
{
    if (f != NULL) {
        xfree(f);
    }
}

static inline char*
memory_address(VALUE self)
{
    return ((AbstractMemory *)DATA_PTR((self)))->address;
}

static inline char*
pointer_native(VALUE value)
{
    const int type = TYPE(value);

    if (rb_obj_is_kind_of(value, rb_FFI_Pointer_class) && type == T_DATA) {
        return memory_address(value);
    } else if (type == T_NIL) {
        return NULL;
    } else if (type == T_FIXNUM) {
        return (char *)(uintptr_t) FIX2INT(value);
    } else if (type == T_BIGNUM) {
        return (char *)(uintptr_t) NUM2ULL(value);
    } else if (rb_respond_to(value, to_ptr)) {
        VALUE ptr = rb_funcall2(value, to_ptr, 0, NULL);
        if (rb_obj_is_kind_of(ptr, rb_FFI_Pointer_class) && TYPE(ptr) == T_DATA) {
            return memory_address(ptr);
        } else {
            rb_raise(rb_eArgError, "to_ptr returned an invalid pointer");
        }
    } else {
        rb_raise(rb_eArgError, "value is not a pointer");
    }
}
static inline VALUE
pointer_new(char* value)
{
    return rb_FFI_Pointer_new(value);
}

static inline char*
string_to_native(VALUE value)
{
    rb_raise(rb_eArgError, "Cannot set :string fields");
}

static inline VALUE
string_from_native(char* value)
{
    return value != NULL ? rb_tainted_str_new2(value) : Qnil;
}

#define FIELD_OP(name, type, toNative, fromNative) \
static VALUE struct_field_put_##name(VALUE self, VALUE pointer, VALUE value); \
static inline void ptr_put_##name(AbstractMemory* ptr, StructField* field, VALUE value); \
static inline VALUE ptr_get_##name(AbstractMemory* ptr, StructField* field); \
static inline void \
ptr_put_##name(AbstractMemory* ptr, StructField* field, VALUE value) \
{ \
    type tmp = toNative(value); \
    memcpy(ptr->address + field->offset, &tmp, sizeof(tmp)); \
} \
static inline VALUE \
ptr_get_##name(AbstractMemory* ptr, StructField* field) \
{ \
    type tmp; \
    memcpy(&tmp, ptr->address + field->offset, sizeof(tmp)); \
    return fromNative(tmp); \
} \
static VALUE \
struct_field_put_##name(VALUE self, VALUE pointer, VALUE value) \
{ \
    StructField* f;  Data_Get_Struct(self, StructField, f); \
    ptr_put_##name(MEMORY(pointer), f, value); \
    return self; \
} \
static VALUE struct_field_get_##name(VALUE self, VALUE pointer); \
static VALUE \
struct_field_get_##name(VALUE self, VALUE pointer) \
{ \
    StructField* f;  Data_Get_Struct(self, StructField, f); \
    return ptr_get_##name(MEMORY(pointer), f); \
}

FIELD_OP(int8, int8_t, NUM2INT, INT2NUM);
FIELD_OP(uint8, uint8_t, NUM2UINT, UINT2NUM);
FIELD_OP(int16, int16_t, NUM2INT, INT2NUM);
FIELD_OP(uint16, uint16_t, NUM2UINT, UINT2NUM);
FIELD_OP(int32, int32_t, NUM2INT, INT2NUM);
FIELD_OP(uint32, uint32_t, NUM2UINT, UINT2NUM);
FIELD_OP(int64, int64_t, NUM2LL, LL2NUM);
FIELD_OP(uint64, uint64_t, NUM2ULL, ULL2NUM);
FIELD_OP(float32, float, NUM2DBL, rb_float_new);
FIELD_OP(float64, double, NUM2DBL, rb_float_new);
FIELD_OP(pointer, char*, pointer_native, pointer_new);
FIELD_OP(string, char*, string_to_native, string_from_native);

static VALUE
struct_new(int argc, VALUE* argv, VALUE klass)
{
    Struct* s;
    VALUE retval, rbPointer = Qnil, rest = Qnil, rbLayout = Qnil;
    int nargs;
    retval = Data_Make_Struct(klass, Struct, struct_mark, struct_free, s);
    s->rbPointer = Qnil;
    s->pointer = NULL;

    nargs = rb_scan_args(argc, argv, "01*", &rbPointer, &rest);
    /* Call up into ruby code to adjust the layout */
    if (nargs > 1) {
        rbLayout = rb_funcall2(klass, rb_intern("layout"), RARRAY_LEN(rest), RARRAY_PTR(rest));
    } else if (rb_cvar_defined(klass, layoutID)) {
        rbLayout = rb_cvar_get(klass, layoutID);
    }
    if (rbLayout == Qnil || !rb_obj_is_kind_of(rbLayout, classStructLayout)) {
        rb_raise(rb_eRuntimeError, "Invalid Struct layout");
    }
    Data_Get_Struct(rbLayout, StructLayout, s->layout);
    s->rbLayout = rbLayout;

    if (rbPointer == Qnil) {
        rbPointer = rb_FFI_MemoryPointer_new(s->layout->size, 1, true);
    }
    
    rb_funcall2(retval, initializeID, 1, &rbPointer);
    if (s->pointer == NULL) {
        rb_raise(rb_eRuntimeError, "Struct memory not set (NULL)");
    }
    return retval;
}

static VALUE
struct_alloc(int argc, VALUE* argv, VALUE klass)
{
    Struct* s;
    VALUE retval, rbPointer = Qnil, rbLayout = Qnil, rbClear = Qnil;

    retval = Data_Make_Struct(klass, Struct, struct_mark, struct_free, s);
    s->rbPointer = Qnil;
    s->pointer = NULL;
    
    if (rb_cvar_defined(klass, layoutID)) {
        rbLayout = rb_cvar_get(klass, layoutID);
    }
    if (rbLayout == Qnil || !rb_obj_is_kind_of(rbLayout, classStructLayout)) {
        rb_raise(rb_eRuntimeError, "Invalid Struct layout");
    }
    Data_Get_Struct(rbLayout, StructLayout, s->layout);
    s->rbLayout = rbLayout;
    rb_scan_args(argc, argv, "01", &rbClear);
    rbPointer = rb_FFI_MemoryPointer_new(s->layout->size, 1, rbClear == Qtrue);

    rb_funcall2(retval, initializeID, 1, &rbPointer);
    if (s->pointer == NULL) {
        rb_raise(rb_eRuntimeError, "Struct memory not set (NULL)");
    }
    return retval;
}

static VALUE
struct_initialize(VALUE self, VALUE rbPointer)
{
    Struct* s;

    Data_Get_Struct(self, Struct, s);

    if (rbPointer == Qnil) {
        rb_raise(rb_eArgError, "Struct memory cannot be NULL");
    }

    if (!rb_obj_is_kind_of(rbPointer, rb_FFI_AbstractMemory_class)) {
        rb_raise(rb_eArgError, "Invalid Struct memory");
    }

    s->rbPointer = rbPointer;
    s->pointer = MEMORY(rbPointer);
    return self;
}

static void
struct_mark(Struct *s)
{
    if (s->rbPointer != Qnil) {
        rb_gc_mark(s->rbPointer);
    }
    if (s->rbLayout != Qnil) {
        rb_gc_mark(s->rbLayout);
    }
}

static void struct_free(Struct *s)
{
    xfree(s);
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
        VALUE str = rb_funcall2(fieldName, rb_intern("to_s"), 0, NULL);
        rb_raise(rb_eArgError, "No such field '%s'", StringValuePtr(str));
    }
    return rbField;
}

static VALUE
struct_get_field(VALUE self, VALUE fieldName)
{
    Struct* s;
    VALUE rbField;
    StructField* f;

    Data_Get_Struct(self, Struct, s);
    rbField = struct_field(s, fieldName);
    f = FIELD_CAST(rbField);

    switch (f->type) {
        case NATIVE_INT8:
            return ptr_get_int8(s->pointer, f);
        case NATIVE_UINT8:
            return ptr_get_uint8(s->pointer, f);
        case NATIVE_INT16:
            return ptr_get_int16(s->pointer, f);
        case NATIVE_UINT16:
            return ptr_get_uint16(s->pointer, f);
        case NATIVE_INT32:
            return ptr_get_int32(s->pointer, f);
        case NATIVE_UINT32:
            return ptr_get_uint32(s->pointer, f);
        case NATIVE_INT64:
            return ptr_get_int64(s->pointer, f);
        case NATIVE_UINT64:
            return ptr_get_uint64(s->pointer, f);
        case NATIVE_FLOAT32:
            return ptr_get_float32(s->pointer, f);
        case NATIVE_FLOAT64:
            return ptr_get_float64(s->pointer, f);
        case NATIVE_POINTER:
            return ptr_get_pointer(s->pointer, f);
        case NATIVE_STRING:
            return ptr_get_string(s->pointer, f);
        default:
            /* call up to the ruby code to fetch the value */
            return rb_funcall2(rbField, getID, 1, &s->rbPointer);
    }
}

static VALUE
struct_put_field(VALUE self, VALUE fieldName, VALUE value)
{
    Struct* s;
    VALUE rbField;
    StructField* f;
    VALUE argv[2];

    Data_Get_Struct(self, Struct, s);
    rbField = struct_field(s, fieldName);
    f = FIELD_CAST(rbField);

    switch (f->type) {
        case NATIVE_INT8:
            ptr_put_int8(s->pointer, f, value);
            break;
        case NATIVE_UINT8:
            ptr_put_uint8(s->pointer, f, value);
            break;
        case NATIVE_INT16:
            ptr_put_int16(s->pointer, f, value);
            break;
        case NATIVE_UINT16:
            ptr_put_uint16(s->pointer, f, value);
            break;
        case NATIVE_INT32:
            ptr_put_int32(s->pointer, f, value);
            break;
        case NATIVE_UINT32:
            ptr_put_uint32(s->pointer, f, value);
            break;
        case NATIVE_INT64:
            ptr_put_int64(s->pointer, f, value);
            break;
        case NATIVE_UINT64:
            ptr_put_uint64(s->pointer, f, value);
            break;
        case NATIVE_FLOAT32:
            ptr_put_float32(s->pointer, f, value);
            break;
        case NATIVE_FLOAT64:
            ptr_put_float64(s->pointer, f, value);
            break;
        case NATIVE_POINTER:
            ptr_put_pointer(s->pointer, f, value);
            break;
        case NATIVE_STRING:
            rb_raise(rb_eArgError, "Cannot set :string fields");
        default:
            /* call up to the ruby code to set the value */
            argv[0] = s->rbPointer;
            argv[1] = value;
            rb_funcall2(rbField, putID, 2, argv);
            break;
    }
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
    rb_ivar_set(self, pointerID, pointer);
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
    rb_ivar_set(self, layoutID, layout);
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
struct_layout_new(VALUE klass, VALUE field_names, VALUE fields, VALUE size, VALUE align)
{
    StructLayout* layout;
    VALUE retval;
    VALUE argv[] = { field_names, fields, size, align };
    retval = Data_Make_Struct(klass, StructLayout, struct_layout_mark, struct_layout_free, layout);
    layout->rbFields = fields;
    layout->size = NUM2INT(size);
    layout->align = NUM2INT(align);
    rb_funcall2(retval, initializeID, sizeof(argv) / sizeof(argv[0]), argv);
    return retval;
}

static void
struct_layout_mark(StructLayout *layout)
{
    if (layout->rbFields != Qnil) {
        rb_gc_mark(layout->rbFields);
    }
}

static void
struct_layout_free(StructLayout *layout)
{
    xfree(layout);
}

static VALUE
struct_layout_get(VALUE self, VALUE field)
{
    StructLayout* layout;

    Data_Get_Struct(self, StructLayout, layout);
    return rb_hash_aref(layout->rbFields, field);
}

void
rb_FFI_Struct_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    VALUE klass;
    rb_FFI_Struct_class = classStruct = rb_define_class_under(moduleFFI, "Struct", rb_cObject);
    classStructLayout = rb_define_class_under(moduleFFI, "StructLayout", rb_cObject);
    classStructLayoutBuilder = rb_define_class_under(moduleFFI, "StructLayoutBuilder", rb_cObject);
    classStructField = rb_define_class_under(classStructLayoutBuilder, "Field", rb_cObject);

    //rb_define_singleton_method(classStructLayoutBuilder, "new", builder_new, 0);
    rb_define_singleton_method(classStruct, "new", struct_new, -1);
    rb_define_singleton_method(classStruct, "__alloc", struct_alloc, -1);
    rb_define_alias(rb_singleton_class(classStruct), "alloc_in", "__alloc");
    rb_define_alias(rb_singleton_class(classStruct), "alloc_out", "__alloc");
    rb_define_alias(rb_singleton_class(classStruct), "alloc_inout", "__alloc");
    rb_define_alias(rb_singleton_class(classStruct), "new_in", "__alloc");
    rb_define_alias(rb_singleton_class(classStruct), "new_out", "__alloc");
    rb_define_alias(rb_singleton_class(classStruct), "new_inout", "__alloc");
    rb_define_method(classStruct, "pointer", struct_get_pointer, 0);
    rb_define_private_method(classStruct, "pointer=", struct_set_pointer, 1);
    rb_define_method(classStruct, "layout", struct_get_layout, 0);
    rb_define_private_method(classStruct, "layout=", struct_set_layout, 1);
    rb_define_private_method(classStruct, "initialize", struct_initialize, 1);
    rb_define_method(classStruct, "[]", struct_get_field, 1);
    rb_define_method(classStruct, "[]=", struct_put_field, 2);
    rb_define_singleton_method(classStructField, "new", struct_field_new, -1);
    rb_define_method(classStructField, "offset", struct_field_offset, 0);
    rb_define_singleton_method(classStructLayout, "new", struct_layout_new, 4);
    rb_define_method(classStructLayout, "[]", struct_layout_get, 1);
    initializeID = rb_intern("initialize");
    pointerID = rb_intern("@pointer");
    layoutID = rb_intern("@layout");
    getID = rb_intern("get");
    putID = rb_intern("put");
    to_ptr = rb_intern("to_ptr");
    SIZE_ID = rb_intern("SIZE");
    ALIGN_ID = rb_intern("ALIGN");
#undef FIELD
#define FIELD(name, typeName, nativeType, T) do { \
    typedef struct { char c; T v; } s; \
    klass = rb_define_class_under(classStructLayoutBuilder, #name, classStructField); \
    rb_define_method(klass, "put", struct_field_put_##typeName, 2); \
    rb_define_method(klass, "get", struct_field_get_##typeName, 1); \
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
