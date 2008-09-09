#include <sys/param.h>
#include <sys/types.h>
#include <stdbool.h>
#include <ruby.h>
#include <ctype.h>
#include "Platform.h"

static VALUE modulePlatform = Qnil;

static const char*
strtolower(char* dst, const char* src)
{
    while ((*dst = tolower(*src)) != '\0') {
        ++dst; ++src;
    }
    return dst;
}

void
rb_FFI_Platform_Init()
{
    char osname[100], arch[100];

    VALUE moduleFFI = rb_define_module("FFI");
    VALUE platform = rb_define_module_under(moduleFFI, "Platform");
    rb_define_const(platform, "LONG_SIZE", INT2FIX(sizeof(long) * 8));
    rb_define_const(platform, "ADDRESS_SIZE", INT2FIX(sizeof(void *) * 8));
    rb_define_const(platform, "BYTE_ORDER", INT2FIX(BYTE_ORDER));
    rb_define_const(platform, "LITTLE_ENDIAN", INT2FIX(LITTLE_ENDIAN));
    rb_define_const(platform, "BIG_ENDIAN", INT2FIX(BIG_ENDIAN));
    strtolower(osname, OS);
    strtolower(arch, ARCH);
    rb_define_const(platform, "OS", rb_str_new2(osname));
    rb_define_const(platform, "ARCH", rb_str_new2(arch));
    modulePlatform = platform;
}