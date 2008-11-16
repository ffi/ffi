#ifndef FFI_COMPAT_H
#define FFI_COMPAT_H

#include <ruby.h>

#ifndef RARRAY_LEN
#  define RARRAY_LEN(ary) RARRAY(ary)->len
#endif
#ifndef RSTRING_LEN
#  define RSTRING_LEN(s) RSTRING(s)->len
#endif
#ifndef RSTRING_LEN
#  define RSTRING_LEN(s) RSTRING(s)->len
#endif
#ifndef RSTRING_PTR
#  define RSTRING_PTR(s) RSTRING(s)->ptr
#endif
#ifndef NUM2ULL
#  define NUM2ULL(x) rb_num2ull((VALUE)x)
#endif

#endif /* FFI_COMPAT_H */
