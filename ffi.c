#include <sys/types.h>
#include <stdio.h>
#include <ruby.h>

#ifndef MACOSX
#  define MACOSX
#endif
#include <ffi/ffi.h>

typedef struct Invoker {
    
} Invoker;

typedef enum {
    VOID,
    INT8,
    UINT8,
    INT16,
    UINT16,
    INT32,
    UINT32,
    INT64,
    UINT64,
    FLOAT,
    DOUBLE,
    POINTER,
    STRING,
} NativeType;

static VALUE create_invoker(VALUE self, VALUE lib, VALUE cname, VALUE parameterTypes, 
        VALUE returnType, VALUE convention);
static VALUE invoker_attach(VALUE self, VALUE mod, VALUE methodName);
static void invoker_mark(Invoker *);
static void invoker_free(Invoker *);

void Init_ffi();

static VALUE moduleFFI = Qnil;
static VALUE classInvoker = Qnil;
static VALUE moduleNativeType = Qnil;

static VALUE
create_invoker(VALUE self, VALUE lib, VALUE cname, VALUE parameterTypes, 
        VALUE returnType, VALUE convention)
{
    Invoker* invoker;
    Check_Type(cname, T_STRING);
    Check_Type(parameterTypes, T_ARRAY);
    Check_Type(returnType, T_FIXNUM);
    Check_Type(convention, T_STRING);
    invoker = calloc(1, sizeof(*invoker));
    return Data_Wrap_Struct(classInvoker, invoker_mark, invoker_free, invoker);
}
static VALUE foo(VALUE self) {
    return Qnil;
}
static VALUE
invoker_attach(VALUE self, VALUE mod, VALUE methodName)
{
    const char* mname = StringValueCStr(methodName);
    Check_Type(mod, T_MODULE);
    Check_Type(methodName, T_STRING);
    printf("mod=%p\n", mod);
    printf("Attaching as %s\n", mname); fflush(stdout);
    rb_define_module_function(mod, mname, foo, 1);
    return Qnil;
}

static void
invoker_mark(Invoker *invoker)
{

}
static void
invoker_free(Invoker *invoker)
{
    
}
void
Init_ffi() {
    moduleFFI = rb_define_module("FFI");
    rb_define_module_function(moduleFFI, "create_invoker", create_invoker, 5);
    classInvoker = rb_define_class_under(moduleFFI, "Invoker", rb_cModule);
    rb_define_method(classInvoker, "attach", invoker_attach, 2);
    moduleNativeType = rb_define_module_under(moduleFFI, "NativeType");
    
    rb_define_const(moduleNativeType, "VOID", VOID);
    rb_define_const(moduleNativeType, "INT8", INT8);
    rb_define_const(moduleNativeType, "UINT8", UINT8);
    rb_define_const(moduleNativeType, "INT16", INT16);
    rb_define_const(moduleNativeType, "UINT16", UINT16);
    rb_define_const(moduleNativeType, "INT32", INT32);
    rb_define_const(moduleNativeType, "UINT32", UINT32);
    rb_define_const(moduleNativeType, "INT64", INT64);
    rb_define_const(moduleNativeType, "UINT64", UINT64);
    rb_define_const(moduleNativeType, "FLOAT", FLOAT);
    rb_define_const(moduleNativeType, "DOUBLE", DOUBLE);
    rb_define_const(moduleNativeType, "POINTER", POINTER);
    rb_define_const(moduleNativeType, "STRING", STRING);
    if (sizeof(long) == 4) {
        rb_define_const(moduleNativeType, "LONG", INT32);
        rb_define_const(moduleNativeType, "ULONG", UINT32);
    } else {
        rb_define_const(moduleNativeType, "LONG", INT64);
        rb_define_const(moduleNativeType, "ULONG", UINT64);
    }
    
    puts("Init_ffi completed");
}