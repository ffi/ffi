#ifndef _RBFFI_H
#define	_RBFFI_H

#include <ruby.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define MAX_PARAMETERS (32)

extern void rb_FFI_Type_Init(VALUE ffiModule);
extern void rb_FFI_AbstractMemory_Init(VALUE ffiModule);
extern void rb_FFI_MemoryPointer_Init(VALUE ffiModule);
extern void rb_FFI_Buffer_Init(VALUE ffiModule);
extern void rb_FFI_Invoker_Init(VALUE ffiModule);
extern VALUE rb_FFI_AbstractMemory_class, rb_FFI_Invoker_class;
extern int rb_FFI_type_size(VALUE type);

#ifdef	__cplusplus
}
#endif

#endif	/* _RBFFI_H */

