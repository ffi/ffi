#ifndef RBFFI_FUNCTION_H
#define	RBFFI_FUNCTION_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "Type.h"
#include <ffi.h>

typedef struct {
    Type type; // The native type of a FunctionInfo object
    VALUE rbReturnType;
    VALUE rbParameterTypes;

    Type* returnType;
    Type** parameterTypes;
    NativeType* nativeParameterTypes;
    ffi_type* ffiReturnType;
    ffi_type** ffiParameterTypes;
    ffi_cif ffi_cif;
    int parameterCount;
    int flags;
    ffi_abi abi;
    int callbackCount;
    VALUE* callbackParameters;
    VALUE rbEnums;
} FunctionInfo;

extern VALUE rbffi_FunctionInfoClass;

void rbffi_Function_Init(VALUE moduleFFI);
void rbffi_FunctionInfo_Init(VALUE moduleFFI);

#ifdef	__cplusplus
}
#endif

#endif	/* RBFFI_FUNCTION_H */

