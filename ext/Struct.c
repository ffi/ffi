#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>
#include <ruby.h>
#include <st.h>
#include "rbffi.h"
#include "compat.h"
#include "AbstractMemory.h"
#include "Pointer.h"
#include "Struct.h"

typedef struct StructField {
    unsigned int type;
    unsigned int offset;
    unsigned int size;
    unsigned int align;
} StructField;

typedef struct StructLayout {
    unsigned int fieldCount;
    st_table* symbolMap;
    st_table* stringMap;
} StructLayout;

typedef struct StructLayoutBuilder {
    unsigned int offset;
    st_table* fieldMap;
} StructLayoutBuilder;

typedef struct Struct {
    StructLayout* layout;
    VALUE rbLayout;
} Struct;

static void struct_field_mark(StructField *);
static void struct_field_free(StructField *);

static VALUE classBaseStruct = Qnil, classStructLayout = Qnil;
static VALUE classStructField = Qnil, classStructLayoutBuilder = Qnil;

static VALUE
struct_layout_builder_new(VALUE self)
{

}

static VALUE
struct_layout_builder_add(int argc, VALUE* argv, VALUE self)
{

}

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
    rb_iv_set(retval, "@off", offset);
    rb_iv_set(retval, "@info", info);
    return retval;
}

static VALUE
struct_field_offset(VALUE self)
{
    StructField* field = (StructField *) DATA_PTR(self);
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

#define NUM_OP(name, type, toNative, fromNative) \
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
    ptr_put_##name((AbstractMemory *) DATA_PTR(pointer), (StructField *) DATA_PTR(self), value); \
    return self; \
} \
static VALUE struct_field_get_##name(VALUE self, VALUE pointer); \
static VALUE \
struct_field_get_##name(VALUE self, VALUE pointer) \
{ \
    return ptr_get_##name((AbstractMemory *) DATA_PTR(pointer), (StructField *) DATA_PTR(self)); \
}
NUM_OP(int8, int8_t, NUM2INT, INT2NUM);
NUM_OP(uint8, u_int8_t, NUM2UINT, UINT2NUM);
NUM_OP(int16, int16_t, NUM2INT, INT2NUM);
NUM_OP(uint16, u_int16_t, NUM2UINT, UINT2NUM);
NUM_OP(int32, int32_t, NUM2INT, INT2NUM);
NUM_OP(uint32, u_int32_t, NUM2UINT, UINT2NUM);
NUM_OP(int64, int64_t, NUM2LL, LL2NUM);
NUM_OP(uint64, u_int64_t, NUM2ULL, ULL2NUM);
NUM_OP(float32, float, NUM2DBL, rb_float_new);
NUM_OP(float64, double, NUM2DBL, rb_float_new);

static VALUE
struct_get_field(VALUE self, VALUE field)
{
    Struct* s = (Struct *) DATA_PTR(self);
    return Qnil;
}

static VALUE
struct_put_field(VALUE self, VALUE field, VALUE value)
{
    Struct* s = (Struct *) DATA_PTR(self);
    return Qtrue;
}

void
rb_FFI_Struct_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    VALUE klass;
    classBaseStruct = rb_define_class_under(moduleFFI, "BaseStruct", rb_cObject);
    classStructLayout = rb_define_class_under(moduleFFI, "StructLayout", rb_cObject);
    classStructLayoutBuilder = rb_define_class_under(moduleFFI, "StructLayoutBuilder", rb_cObject);
    classStructField = rb_define_class_under(classStructLayoutBuilder, "Field", rb_cObject);

    //rb_define_singleton_method(classStructLayoutBuilder, "new", builder_new, 0);
    rb_define_singleton_method(classStructField, "new", struct_field_new, -1);
    rb_define_method(classStructField, "offset", struct_field_offset, 0);
#undef NUM_OP
#define NUM_OP(name, typeName, T) do { \
    typedef struct { char c; T v; } s; \
    klass = rb_define_class_under(classStructLayoutBuilder, #name, classStructField); \
    rb_define_method(klass, "put", struct_field_put_##typeName, 2); \
    rb_define_method(klass, "get", struct_field_get_##typeName, 1); \
    rb_define_const(klass, "ALIGN", INT2NUM((sizeof(s) - sizeof(T)) * 8)); \
    rb_define_const(klass, "SIZE", INT2NUM(sizeof(T)* 8)); \
    } while(0)
    
    NUM_OP(Signed8, int8, char);
    NUM_OP(Unsigned8, uint8, unsigned char);
    NUM_OP(Signed16, int16, short);
    NUM_OP(Unsigned16, uint16, unsigned short);
    NUM_OP(Signed32, int32, int);
    NUM_OP(Unsigned32, uint32, unsigned int);
    NUM_OP(Signed64, int64, long long);
    NUM_OP(Unsigned64, uint64, unsigned long long);
    NUM_OP(FloatField, float32, float);
    NUM_OP(DoubleField, float64, double);
}