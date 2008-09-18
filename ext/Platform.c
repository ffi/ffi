#include <sys/param.h>
#include <sys/types.h>
#include <stdbool.h>
#include <ruby.h>
#include <ctype.h>
#include "Platform.h"

static VALUE modulePlatform = Qnil;

void
rb_FFI_Platform_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    VALUE platform = rb_define_module_under(moduleFFI, "Platform");
    rb_define_const(platform, "LONG_SIZE", INT2FIX(sizeof(long) * 8));
    rb_define_const(platform, "ADDRESS_SIZE", INT2FIX(sizeof(void *) * 8));
    rb_define_const(platform, "BYTE_ORDER", INT2FIX(BYTE_ORDER));
    rb_define_const(platform, "LITTLE_ENDIAN", INT2FIX(LITTLE_ENDIAN));
    rb_define_const(platform, "BIG_ENDIAN", INT2FIX(BIG_ENDIAN));
    rb_define_const(platform, "OS_", rb_str_new2(OS));
    rb_define_const(platform, "ARCH_", rb_str_new2(ARCH));
    modulePlatform = platform;
}