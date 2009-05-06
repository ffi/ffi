
#ifndef _MEMORYPOINTER_H
#define	_MEMORYPOINTER_H

#include <stdbool.h>
#include <ruby.h>

#ifdef	__cplusplus
extern "C" {
#endif

    extern void rb_FFI_MemoryPointer_Init(VALUE moduleFFI);
    extern VALUE rb_FFI_MemoryPointer_class;
    extern VALUE rb_FFI_MemoryPointer_new(long size, long count, bool clear);
#ifdef	__cplusplus
}
#endif

#endif	/* _MEMORYPOINTER_H */

