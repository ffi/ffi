
#ifndef _LIBRARY_H
#define	_LIBRARY_H

#ifdef	__cplusplus
extern "C" {
#endif



typedef struct Library {
    void* handle;
} Library;

extern void rb_FFI_NativeLibrary_Init();

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBRARY_H */

