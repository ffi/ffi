#include <sys/param.h>
#include <sys/types.h>
#ifndef _WIN32
#  include <sys/mman.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <ruby.h>
#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32) && !defined(__WIN32__)
# include <pthread.h>
#endif

#include <ffi.h>
#include "rbffi.h"
#include "compat.h"

#include "AbstractMemory.h"
#include "Pointer.h"
#include "Struct.h"
#include "Platform.h"
#include "Callback.h"
#include "Types.h"
#include "Type.h"
#include "LastError.h"
#include "Function.h"

VALUE rbffi_FunctionClass;

void
rbffi_Function_Init(VALUE moduleFFI)
{
    rbffi_FunctionClass = rb_define_class_under(moduleFFI, "Function", rbffi_PointerClass);
    rb_global_variable(&rbffi_FunctionClass);

}

