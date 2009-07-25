#include <sys/param.h>
#include <sys/types.h>
#ifndef _WIN32
#  include <sys/mman.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <ruby.h>
#if defined(HAVE_NATIVETHREAD) && !defined(_WIN32) && !defined(__WIN32__)
# include <pthread.h>
#endif

#include <ffi.h>
#include "rbffi.h"
#include "compat.h"

#include "AbstractMemory.h"
#include "Pointer.h"
#include "Struct.h"
#include "Platform.h"
#include "Function.h"
#include "Callback.h"
#include "Types.h"
#include "Type.h"
#include "LastError.h"
#include "Call.h"

#ifdef USE_RAW
#  ifndef __i386__
#    error "RAW argument packing only supported on i386"
#  endif

#define INT8_SIZE (sizeof(char))
#define INT16_SIZE (sizeof(short))
#define INT32_SIZE (sizeof(int))
#define INT64_SIZE (sizeof(long long))
#define FLOAT32_SIZE (sizeof(float))
#define FLOAT64_SIZE (sizeof(double))
#define ADDRESS_SIZE (sizeof(void *))
#define INT8_ADJ (4)
#define INT16_ADJ (4)
#define INT32_ADJ (4)
#define INT64_ADJ (8)
#define FLOAT32_ADJ (4)
#define FLOAT64_ADJ (8)
#define ADDRESS_ADJ (sizeof(void *))

#endif /* USE_RAW */

#ifdef USE_RAW
#  define ADJ(p, a) ((p) = (FFIStorage*) (((char *) p) + a##_ADJ))
#else
#  define ADJ(p, a) (++(p))
#endif

static void* callback_param(VALUE proc, VALUE cbinfo);
static inline int getSignedInt(VALUE value, int type, int minValue, int maxValue, const char* typeName, VALUE enums);
static inline int getUnsignedInt(VALUE value, int type, int maxValue, const char* typeName);
static inline unsigned int getUnsignedInt32(VALUE value, int type);

static ID id_to_ptr, id_map_symbol;

void
rbffi_SetupCallParams(int argc, VALUE* argv, int paramCount, NativeType* paramTypes,
        FFIStorage* paramStorage, void** ffiValues,
        VALUE* callbackParameters, int callbackCount, VALUE enums)
{
    VALUE callbackProc = Qnil;
    FFIStorage* param = &paramStorage[0];
    int i, argidx, cbidx, argCount;

    if (paramCount != -1 && paramCount != argc) {
        if (argc == (paramCount - 1) && callbackCount == 1 && rb_block_given_p()) {
            callbackProc = rb_block_proc();
        } else {
            rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", argc, paramCount);
        }
    }
    argCount = paramCount != -1 ? paramCount : argc;
    for (i = 0, argidx = 0, cbidx = 0; i < argCount; ++i) {
        int type = argidx < argc ? TYPE(argv[argidx]) : T_NONE;
        ffiValues[i] = param;
        switch (paramTypes[i]) {
            case NATIVE_INT8:
                param->s8 = getSignedInt(argv[argidx++], type, -128, 127, "char", Qnil);
                ADJ(param, INT8);
                break;
            case NATIVE_INT16:
                param->s16 = getSignedInt(argv[argidx++], type, -0x8000, 0x7fff, "short", Qnil);
                ADJ(param, INT16);
                break;
            case NATIVE_INT32:
            case NATIVE_ENUM:
                param->s32 = getSignedInt(argv[argidx++], type, -0x80000000, 0x7fffffff, "int", enums);
                ADJ(param, INT32);
                break;
            case NATIVE_BOOL:
                if (type != T_TRUE && type != T_FALSE) {
                    rb_raise(rb_eTypeError, "Expected a Boolean parameter");
                }
                param->s32 = argv[argidx++] == Qtrue;
                ADJ(param, INT32);
                break;
            case NATIVE_UINT8:
                param->u8 = getUnsignedInt(argv[argidx++], type, 0xff, "unsigned char");
                ADJ(param, INT8);
                break;
            case NATIVE_UINT16:
                param->u16 = getUnsignedInt(argv[argidx++], type, 0xffff, "unsigned short");
                ADJ(param, INT16);
                break;
            case NATIVE_UINT32:
                /* Special handling/checking for unsigned 32 bit integers */
                param->u32 = getUnsignedInt32(argv[argidx++], type);
                ADJ(param, INT32);
                break;
            case NATIVE_INT64:
                if (type != T_FIXNUM && type != T_BIGNUM) {
                    rb_raise(rb_eTypeError, "Expected an Integer parameter");
                }
                param->i64 = NUM2LL(argv[argidx]);
                ADJ(param, INT64);
                ++argidx;
                break;
            case NATIVE_UINT64:
                if (type != T_FIXNUM && type != T_BIGNUM) {
                    rb_raise(rb_eTypeError, "Expected an Integer parameter");
                }
                param->u64 = NUM2ULL(argv[argidx]);
                ADJ(param, INT64);
                ++argidx;
                break;
            case NATIVE_FLOAT32:
                if (type != T_FLOAT && type != T_FIXNUM) {
                    rb_raise(rb_eTypeError, "Expected a Float parameter");
                }
                param->f32 = (float) NUM2DBL(argv[argidx]);
                ADJ(param, FLOAT32);
                ++argidx;
                break;
            case NATIVE_FLOAT64:
                if (type != T_FLOAT && type != T_FIXNUM) {
                    rb_raise(rb_eTypeError, "Expected a Float parameter");
                }
                param->f64 = NUM2DBL(argv[argidx]);
                ADJ(param, FLOAT64);
                ++argidx;
                break;
            case NATIVE_STRING:
                if (type == T_STRING) {
                    if (rb_safe_level() >= 1 && OBJ_TAINTED(argv[argidx])) {
                        rb_raise(rb_eSecurityError, "Unsafe string parameter");
                    }
                    param->ptr = StringValueCStr(argv[argidx]);
                } else if (type == T_NIL) {
                    param->ptr = NULL;
                } else {
                    rb_raise(rb_eArgError, "Invalid String value");
                }
                ADJ(param, ADDRESS);
                ++argidx;
                break;
            case NATIVE_POINTER:
            case NATIVE_BUFFER_IN:
            case NATIVE_BUFFER_OUT:
            case NATIVE_BUFFER_INOUT:
                if (type == T_DATA && rb_obj_is_kind_of(argv[argidx], rbffi_AbstractMemoryClass)) {
                    param->ptr = ((AbstractMemory *) DATA_PTR(argv[argidx]))->address;
                } else if (type == T_DATA && rb_obj_is_kind_of(argv[argidx], rbffi_StructClass)) {
                    AbstractMemory* memory = ((Struct *) DATA_PTR(argv[argidx]))->pointer;
                    param->ptr = memory != NULL ? memory->address : NULL;
                } else if (type == T_STRING) {
                    if (rb_safe_level() >= 1 && OBJ_TAINTED(argv[argidx])) {
                        rb_raise(rb_eSecurityError, "Unsafe string parameter");
                    }
                    param->ptr = StringValuePtr(argv[argidx]);
                } else if (type == T_NIL) {
                    param->ptr = NULL;
                } else if (rb_respond_to(argv[argidx], id_to_ptr)) {
                    VALUE ptr = rb_funcall2(argv[argidx], id_to_ptr, 0, NULL);
                    if (rb_obj_is_kind_of(ptr, rbffi_AbstractMemoryClass) && TYPE(ptr) == T_DATA) {
                        param->ptr = ((AbstractMemory *) DATA_PTR(ptr))->address;
                    } else {
                        rb_raise(rb_eArgError, "to_ptr returned an invalid pointer");
                    }

                } else {
                    rb_raise(rb_eArgError, ":pointer argument is not a valid pointer");
                }
                ADJ(param, ADDRESS);
                ++argidx;
                break;
            case NATIVE_FUNCTION:
            case NATIVE_CALLBACK:
                if (callbackProc != Qnil) {
                    param->ptr = callback_param(callbackProc, callbackParameters[cbidx++]);
                } else {
                    param->ptr = callback_param(argv[argidx], callbackParameters[cbidx++]);
                    ++argidx;
                }
                ADJ(param, ADDRESS);
                break;
            default:
                rb_raise(rb_eArgError, "Invalid parameter type: %d", paramTypes[i]);
        }
    }
}

static inline int
getSignedInt(VALUE value, int type, int minValue, int maxValue, const char* typeName, VALUE enums)
{
    int i;
    if (type == T_SYMBOL && enums != Qnil) {
        value = rb_funcall2(enums, id_map_symbol, 1, &value);
        if (value == Qnil) {
            rb_raise(rb_eTypeError, "Expected a valid enum constant");
        }
    }
    else if (type != T_FIXNUM && type != T_BIGNUM) {
        rb_raise(rb_eTypeError, "Expected an Integer parameter");
    }
    i = NUM2INT(value);
    if (i < minValue || i > maxValue) {
        rb_raise(rb_eRangeError, "Value %d outside %s range", i, typeName);
    }
    return i;
}

static inline int
getUnsignedInt(VALUE value, int type, int maxValue, const char* typeName)
{
    int i;
    if (type != T_FIXNUM && type != T_BIGNUM) {
        rb_raise(rb_eTypeError, "Expected an Integer parameter");
    }
    i = NUM2INT(value);
    if (i < 0 || i > maxValue) {
        rb_raise(rb_eRangeError, "Value %d outside %s range", i, typeName);
    }
    return i;
}

/* Special handling/checking for unsigned 32 bit integers */
static inline unsigned int
getUnsignedInt32(VALUE value, int type)
{
    long long i;
    if (type != T_FIXNUM && type != T_BIGNUM) {
        rb_raise(rb_eTypeError, "Expected an Integer parameter");
    }
    i = NUM2LL(value);
    if (i < 0L || i > 0xffffffffL) {
        rb_raise(rb_eRangeError, "Value %lld outside unsigned int range", i);
    }
    return (unsigned int) i;
}


static void*
callback_param(VALUE proc, VALUE cbInfo)
{
    VALUE callback ;
    if (proc == Qnil) {
        return NULL ;
    }
    callback = rbffi_NativeCallback_ForProc(proc, cbInfo);
    return ((NativeCallback *) DATA_PTR(callback))->code;
}


void
rbffi_Call_Init(VALUE moduleFFI)
{
    id_to_ptr = rb_intern("to_ptr");
    id_map_symbol = rb_intern("__map_symbol");
}

