/* 
 * File:   MemoryPointer.h
 * Author: wayne
 *
 * Created on August 28, 2008, 5:24 PM
 */

#ifndef _MEMORYPOINTER_H
#define	_MEMORYPOINTER_H

#include <ruby.h>

#ifdef	__cplusplus
extern "C" {
#endif

    extern void rb_FFI_MemoryPointer_Init();
    extern VALUE rb_FFI_MemoryPointer_new(caddr_t addr);
    extern VALUE rb_FFI_MemoryPointer_class;
#ifdef	__cplusplus
}
#endif

#endif	/* _MEMORYPOINTER_H */

