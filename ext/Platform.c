#include <sys/param.h>
#include <sys/types.h>
#include <stdbool.h>
#include <ruby.h>
#include <ctype.h>
#include "Platform.h"

static VALUE modulePlatform = Qnil;

/*
 * Determine the cpu type at compile time - useful for MacOSX where the the
 * system installed ruby incorrectly reports 'host_cpu' as 'powerpc' when running
 * on intel.
 */
#ifdef __i386__
#define CPU "i386"
#elif defined(__ppc__) || defined(__powerpc__)
#define CPU "powerpc"
#elif defined(__x86_64__)
#define CPU "x86_64"
#elif defined(__sparc__)
#define CPU "sparc"
#elif defined(__sparcv9__)
#define CPU "sparcv9"
#else
#error "Unknown cpu type"
#endif

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
    rb_define_const(platform, "ARCH_", rb_str_new2(CPU));
    modulePlatform = platform;
}