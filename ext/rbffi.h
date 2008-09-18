/* 
 * File:   rbffi.h
 * Author: wayne
 *
 * Created on August 27, 2008, 5:40 PM
 */

#ifndef _RBFFI_H
#define	_RBFFI_H

#include <ruby.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define MAX_PARAMETERS (32)
    
extern void rb_FFI_AbstractMemory_Init();
extern void rb_FFI_MemoryPointer_Init();
extern void rb_FFI_Buffer_Init();
extern void rb_FFI_Callback_Init();
extern void rb_FFI_Invoker_Init();
extern VALUE rb_FFI_AbstractMemory_class;


#ifdef	__cplusplus
}
#endif

#endif	/* _RBFFI_H */

