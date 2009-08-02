#ifndef FFI_COMPAT_H
#define FFI_COMPAT_H

#include <ruby.h>

#ifndef RARRAY_LEN
#  define RARRAY_LEN(ary) RARRAY(ary)->len
#endif
#ifndef RARRAY_PTR
#  define RARRAY_PTR(ary) RARRAY(ary)->ptr
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

#ifndef roundup
#  define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif

#ifdef __GNUC__
#  define likely(x) __builtin_expect((x), 1)
#  define unlikely(x) __builtin_expect((x), 0)
#else
#  define likely(x) (x)
#  define unlikely(x) (x)
#endif

#endif /* FFI_COMPAT_H */
