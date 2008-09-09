/* 
 * File:   Types.h
 * Author: wayne
 *
 * Created on September 9, 2008, 9:47 PM
 */

#ifndef _TYPES_H
#define	_TYPES_H

#ifdef	__cplusplus
extern "C" {
#endif
    
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

#include <ffi.h>
extern ffi_type* rb_ffi_NativeTypeToFFI(NativeType type);
VALUE rb_ffi_NativeValueToRuby(NativeType type, const void* ptr);

#ifdef	__cplusplus
}
#endif

#endif	/* _TYPES_H */

