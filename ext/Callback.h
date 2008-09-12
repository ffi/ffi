/* 
 * File:   Callback.h
 * Author: wayne
 *
 * Created on September 11, 2008, 10:01 AM
 */

#ifndef _CALLBACK_H
#define	_CALLBACK_H

#include "Types.h"

#ifdef	__cplusplus
extern "C" {
#endif
#include <ffi.h>
    
typedef struct {
    NativeType returnType;
    NativeType* parameterTypes;
    ffi_type* ffiReturnType;
    ffi_type** ffiParameterTypes;
    ffi_cif ffi_cif;
    int parameterCount;
    int flags;
    ffi_abi abi;
} CallbackInfo;

typedef struct {
    void* code;
    ffi_closure* ffi_closure;
    ffi_cif ffi_cif;
    int flags;
    CallbackInfo* cbInfo;
    VALUE rbCallbackInfo;
    VALUE rbProc;
} NativeCallback;

#ifdef obsolete
static inline void
callbackinfo_copy(CallbackInfo* dst, const CallbackInfo *src)
{
    int i;
    dst->returnType = src->returnType;
    dst->parameterCount = src->parameterCount;
    dst->parameterTypes = ALLOC_N(NativeType, src->parameterCount);
    dst->flags = src->flags;
    for (i = 0; i < src->parameterCount; ++i) {
        dst->parameterTypes[i] = src->parameterTypes[i];
    }
}
#endif // obsolete

extern VALUE rb_FFI_Callback_class;
extern VALUE rb_FFI_NativeCallback_new(VALUE, VALUE);

#ifdef	__cplusplus
}
#endif

#endif	/* _CALLBACK_H */

