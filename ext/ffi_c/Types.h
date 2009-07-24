#ifndef _TYPES_H
#define	_TYPES_H

#ifdef	__cplusplus
extern "C" {
#endif
#ifdef VOID
#  undef VOID
#endif
typedef enum {
    NATIVE_VOID,
    NATIVE_INT8,
    NATIVE_UINT8,
    NATIVE_INT16,
    NATIVE_UINT16,
    NATIVE_INT32,
    NATIVE_UINT32,
    NATIVE_INT64,
    NATIVE_UINT64,
    NATIVE_FLOAT32,
    NATIVE_FLOAT64,
    NATIVE_POINTER,
    NATIVE_CALLBACK,
    NATIVE_FUNCTION,
    NATIVE_BUFFER_IN,
    NATIVE_BUFFER_OUT,
    NATIVE_BUFFER_INOUT,
    NATIVE_CHAR_ARRAY,
    NATIVE_BOOL,
    
    /**
     * An immutable string.  Nul terminated, but only copies in to the native function
     */
    NATIVE_STRING,
    /** A Rubinus :string arg - copies data both ways, and nul terminates */
    NATIVE_RBXSTRING,
    /** The function takes a variable number of arguments */
    NATIVE_VARARGS,
    /** A typedef-ed enum */
    NATIVE_ENUM,
} NativeType;

#include <ffi.h>
#include "Type.h"

extern ffi_type* rbffi_NativeType_ToFFI(NativeType type);
VALUE rbffi_NativeValue_ToRuby(Type* type, VALUE rbType, const void* ptr, VALUE enums);
void rbffi_Types_Init(VALUE moduleFFI);

#ifdef	__cplusplus
}
#endif

#endif	/* _TYPES_H */

