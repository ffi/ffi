#ifndef _ABSTRACTMEMORY_H
#define	_ABSTRACTMEMORY_H

#include <sys/param.h>
#include <sys/types.h>
#include <stdint.h>

#include "compat.h"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct AbstractMemory_ AbstractMemory;

typedef struct {
    VALUE (*get)(AbstractMemory* ptr, long offset);
    void (*put)(AbstractMemory* ptr, long offset, VALUE value);
} MemoryOp;

typedef struct {
    MemoryOp* int8;
    MemoryOp* uint8;
    MemoryOp* int16;
    MemoryOp* uint16;
    MemoryOp* int32;
    MemoryOp* uint32;
    MemoryOp* int64;
    MemoryOp* uint64;
    MemoryOp* float32;
    MemoryOp* float64;
    MemoryOp* pointer;
    MemoryOp* strptr;
} MemoryOps;

struct AbstractMemory_ {
    char* address; // Use char* instead of void* to ensure adding to it works correctly
    long size;
    MemoryOps* ops;
};


static inline void
checkBounds(AbstractMemory* mem, long off, long len)
{
    if ((off | len | (off + len) | (mem->size - (off + len))) < 0) {
        rb_raise(rb_eIndexError, "Memory access offset=%ld size=%ld is out of bounds",
                off, len);
    }
}

#define MEMORY(obj) rbffi_AbstractMemory_Cast((obj), rbffi_AbstractMemoryClass)
#define MEMORY_PTR(obj) MEMORY((obj))->address
#define MEMORY_LEN(obj) MEMORY((obj))->size

extern void rbffi_AbstractMemory_Init(VALUE ffiModule);

extern AbstractMemory* rbffi_AbstractMemory_Cast(VALUE obj, VALUE klass);

extern VALUE rbffi_AbstractMemoryClass;
extern MemoryOps rbffi_AbstractMemoryOps;

#ifdef	__cplusplus
}
#endif

#endif	/* _ABSTRACTMEMORY_H */

