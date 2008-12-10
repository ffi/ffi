
#ifndef _POINTER_H
#define	_POINTER_H

#ifdef	__cplusplus
extern "C" {
#endif



extern void rb_FFI_Pointer_Init(void);
extern void rb_FFI_NullPointer_Init();
extern VALUE rb_FFI_Pointer_new(void* addr);
extern VALUE rb_FFI_Pointer_class;
extern VALUE rb_FFI_NullPointer_class;
extern VALUE rb_FFI_NullPointer_singleton;

#ifdef	__cplusplus
}
#endif

#endif	/* _POINTER_H */

