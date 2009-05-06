#ifndef _RBFFI_H
#define	_RBFFI_H

#include <ruby.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define MAX_PARAMETERS (32)
    
extern void rb_FFI_AbstractMemory_Init(void);
extern void rb_FFI_MemoryPointer_Init(void);
extern void rb_FFI_Buffer_Init(void);
extern void rb_FFI_Callback_Init(void);
extern void rb_FFI_Invoker_Init(void);
extern VALUE rb_FFI_AbstractMemory_class;
extern int rb_FFI_type_size(VALUE type);

#ifdef	__cplusplus
}
#endif

#endif	/* _RBFFI_H */

