
#ifndef _POINTER_H
#define	_POINTER_H

#ifdef	__cplusplus
extern "C" {
#endif



extern void rb_FFI_Pointer_Init(void);
extern VALUE rb_FFI_Pointer_new(caddr_t addr);
extern VALUE rb_FFI_Pointer_class;


#ifdef	__cplusplus
}
#endif

#endif	/* _POINTER_H */

