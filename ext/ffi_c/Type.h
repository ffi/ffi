#ifndef _TYPE_H
#define	_TYPE_H

#include <ruby.h>
#include <ffi.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct Type_ Type;

#include "Types.h"
    
struct Type_ {
    NativeType nativeType;
    ffi_type* ffiType;
    int size;
    int alignment;
};

extern VALUE rbffi_TypeClass;
extern int rbffi_Type_GetIntValue(VALUE type);
extern VALUE rbffi_Type_Lookup(VALUE type);

#ifdef	__cplusplus
}
#endif

#endif	/* _TYPE_H */

