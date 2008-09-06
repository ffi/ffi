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
    
typedef enum {
    VOID,
    INT8,
    UINT8,
    INT16,
    UINT16,
    INT32,
    UINT32,
    INT64,
    UINT64,
    FLOAT32,
    FLOAT64,
    POINTER,
    CALLBACK,
    BUFFER_IN,
    BUFFER_OUT,
    BUFFER_INOUT,
    CHAR_ARRAY,
    
    /**
     * An immutable string.  Nul terminated, but only copies in to the native function
     */
    STRING,
    /** A Rubinus :string arg - copies data both ways, and nul terminates */
    RBXSTRING
} NativeType;

extern void rb_FFI_AbstractMemory_Init();
extern void rb_FFI_MemoryPointer_Init();
extern void rb_FFI_Callback_Init();
extern VALUE rb_FFI_AbstractMemory_class;

#ifdef	__cplusplus
}
#endif

#endif	/* _RBFFI_H */

