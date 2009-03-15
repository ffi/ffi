#ifndef _ABSTRACTMEMORY_H
#define	_ABSTRACTMEMORY_H

#include <sys/param.h>
#include <sys/types.h>
#include <stdint.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct {
    char* address; // Use char* instead of void* to ensure adding to it works correctly
    long size;
} AbstractMemory;

static inline void
checkBounds(AbstractMemory* mem, long off, long len)
{
    if ((off | len | (off + len) | (mem->size - (off + len))) < 0) {
        rb_raise(rb_eIndexError, "Memory access offset=%ld size=%ld is out of bounds",
                off, len);
    }
}

#define MEMORY(obj) rb_FFI_AbstractMemory_cast((obj), rb_FFI_AbstractMemory_class)
#define MEMORY_PTR(obj) MEMORY((obj))->address
#define MEMORY_LEN(obj) MEMORY((obj))->size

extern AbstractMemory* rb_FFI_AbstractMemory_cast(VALUE obj, VALUE klass);

extern VALUE rb_FFI_AbstractMemory_class;
#ifdef	__cplusplus
}
#endif

#endif	/* _ABSTRACTMEMORY_H */

