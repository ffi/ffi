#include <sys/types.h>
#include <ruby.h>
#include "rbffi.h"

typedef struct {
    NativeType returnType;
    NativeType* parameterTypes;
    int parameterCount;
    VALUE proc;
} Callback;

static VALUE classCallback = Qnil;
static VALUE classCallbackImpl = Qnil;

static VALUE
callback_new(VALUE self, VALUE proc)
{
    return Qnil;
}

void rb_FFI_Callback_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    classCallback = rb_define_class_under(moduleFFI, "Callback", rb_cObject);
    rb_define_singleton_method(classCallback, "new", callback_new, -1);
}