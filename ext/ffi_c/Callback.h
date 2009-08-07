#ifndef _CALLBACK_H
#define	_CALLBACK_H

#include "Types.h"
#include "Type.h"

#ifdef	__cplusplus
extern "C" {
#endif
#include <ffi.h>
#include "Function.h"

typedef struct {
    void* code;
    ffi_closure* ffi_closure;
    ffi_cif ffi_cif;
    int flags;
    FunctionType* cbInfo;
    VALUE rbFunctionInfo;
    VALUE rbProc;
} NativeCallback;

extern void rbffi_Callback_Init(VALUE ffiModule);
extern VALUE rbffi_NativeCallback_NewInstance(VALUE, VALUE);
extern VALUE rbffi_NativeCallback_ForProc(VALUE proc, VALUE cbInfo);

#ifdef	__cplusplus
}
#endif

#endif	/* _CALLBACK_H */

