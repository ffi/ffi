#ifndef _STRUCT_H
#define	_STRUCT_H

#include "AbstractMemory.h"

#ifdef	__cplusplus
extern "C" {
#endif

    extern void rbffi_Struct_Init(VALUE ffiModule);

    struct StructLayout;
    typedef struct Struct {
        struct StructLayout* layout;
        AbstractMemory* pointer;
        VALUE rbLayout;
        VALUE rbPointer;
    } Struct;

    extern VALUE rbffi_StructClass;
#ifdef	__cplusplus
}
#endif

#endif	/* _STRUCT_H */

