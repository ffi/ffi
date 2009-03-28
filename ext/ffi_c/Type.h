#ifndef _TYPE_H
#define	_TYPE_H

#include <ruby.h>

#ifdef	__cplusplus
extern "C" {
#endif

    
typedef struct Type_ {
    NativeType nativeType;
} Type;

    extern VALUE rb_FFI_Type_class;
    extern int rb_FFI_TypeGetIntValue(VALUE type);


#ifdef	__cplusplus
}
#endif

#endif	/* _TYPE_H */

