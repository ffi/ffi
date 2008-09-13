/* 
 * File:   AbstractMemory.h
 * Author: wayne
 *
 * Created on August 28, 2008, 5:52 PM
 */

#ifndef _ABSTRACTMEMORY_H
#define	_ABSTRACTMEMORY_H

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct {
    caddr_t address;
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

#ifdef	__cplusplus
}
#endif

#endif	/* _ABSTRACTMEMORY_H */

