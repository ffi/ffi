
#ifndef _POINTER_H
#define	_POINTER_H

#ifdef	__cplusplus
extern "C" {
#endif



extern void rbffi_Pointer_Init(VALUE moduleFFI);
extern void rbffi_NullPointer_Init(VALUE moduleFFI);
extern VALUE rbffi_Pointer_NewInstance(void* addr);
extern VALUE rbffi_PointerClass;
extern VALUE rbffi_NullPointerClass;
extern VALUE rbffi_NullPointerSingleton;

#ifdef	__cplusplus
}
#endif

#endif	/* _POINTER_H */

