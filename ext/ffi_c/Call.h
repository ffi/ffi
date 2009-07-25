
#ifndef _INVOKE_H
#define	_INVOKE_H

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__i386__) && !defined(_WIN32) && !defined(__WIN32__)
#  define USE_RAW
#endif

#define MAX_FIXED_ARITY (3)
    
typedef union {
#ifdef USE_RAW
    signed int s8, s16, s32;
    unsigned int u8, u16, u32;
#else
    signed char s8;
    unsigned char u8;
    signed short s16;
    unsigned short u16;
    signed int s32;
    unsigned int u32;
#endif
    signed long long i64;
    unsigned long long u64;
    void* ptr;
    float f32;
    double f64;
} FFIStorage;


void rbffi_Call_Init(VALUE moduleFFI);

void
rbffi_SetupCallParams(int argc, VALUE* argv, int paramCount, NativeType* paramTypes,
        FFIStorage* paramStorage, void** ffiValues,
        VALUE* callbackParameters, int callbackCount, VALUE enums);

#ifdef	__cplusplus
}
#endif

#endif	/* _INVOKE_H */

