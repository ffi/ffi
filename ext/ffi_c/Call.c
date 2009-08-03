/*
 * Copyright (c) 2009, Wayne Meissner
 * Copyright (c) 2009, Luc Heinrich <luc@honk-honk.com>
 * Copyright (c) 2009, Mike Dalessio <mike.dalessio@gmail.com>
 * Copyright (c) 2009, Aman Gupta.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * The name of the author or authors may not be used to endorse or promote
 *   products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <ruby.h>

#include <ffi.h>
#include "rbffi.h"
#include "compat.h"

#include "AbstractMemory.h"
#include "Pointer.h"
#include "Struct.h"
#include "Function.h"
#include "Callback.h"
#include "Type.h"
#include "LastError.h"
#include "Call.h"

#ifdef USE_RAW
#  ifndef __i386__
#    error "RAW argument packing only supported on i386"
#  endif

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
static inline void* getPointer(VALUE value, int type);
static inline char* getString(VALUE value, int type);

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
                param->ptr = getString(argv[argidx++], type);
                ADJ(param, ADDRESS);
                break;

            case NATIVE_POINTER:
            case NATIVE_BUFFER_IN:
            case NATIVE_BUFFER_OUT:
            case NATIVE_BUFFER_INOUT:
                param->ptr = getPointer(argv[argidx++], type);
                ADJ(param, ADDRESS);
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


VALUE
rbffi_CallFunction(int argc, VALUE* argv, void* function, FunctionInfo* fnInfo)
{
    FFIStorage retval;
    void** ffiValues;
    FFIStorage* params;

    ffiValues = ALLOCA_N(void *, fnInfo->parameterCount);
    params = ALLOCA_N(FFIStorage, fnInfo->parameterCount);

    rbffi_SetupCallParams(argc, argv,
        fnInfo->parameterCount, fnInfo->nativeParameterTypes, params, ffiValues,
        fnInfo->callbackParameters, fnInfo->callbackCount, fnInfo->rbEnums);

#ifdef USE_RAW
    ffi_raw_call(&fnInfo->ffi_cif, FFI_FN(function), &retval, (ffi_raw *) ffiValues[0]);
#else
    ffi_call(&fnInfo->ffi_cif, FFI_FN(function), &retval, ffiValues);
#endif

    if (!fnInfo->ignoreErrno) {
        rbffi_save_errno();
    }

    return rbffi_NativeValue_ToRuby(fnInfo->returnType, fnInfo->rbReturnType, &retval,
        fnInfo->rbEnums);
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
    
    } else if (type != T_FIXNUM && type != T_BIGNUM) {
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

static inline void*
getPointer(VALUE value, int type)
{
    if (type == T_DATA && rb_obj_is_kind_of(value, rbffi_AbstractMemoryClass)) {

        return ((AbstractMemory *) DATA_PTR(value))->address;

    } else if (type == T_DATA && rb_obj_is_kind_of(value, rbffi_StructClass)) {

        AbstractMemory* memory = ((Struct *) DATA_PTR(value))->pointer;
        return memory != NULL ? memory->address : NULL;

    } else if (type == T_STRING) {

        if (rb_safe_level() >= 1 && OBJ_TAINTED(value)) {
            rb_raise(rb_eSecurityError, "Unsafe string parameter");
        }
        return StringValuePtr(value);

    } else if (type == T_NIL) {

        return NULL;

    } else if (rb_respond_to(value, id_to_ptr)) {

        VALUE ptr = rb_funcall2(value, id_to_ptr, 0, NULL);
        if (rb_obj_is_kind_of(ptr, rbffi_AbstractMemoryClass) && TYPE(ptr) == T_DATA) {
            return ((AbstractMemory *) DATA_PTR(ptr))->address;
        }
        rb_raise(rb_eArgError, "to_ptr returned an invalid pointer");
    }

    rb_raise(rb_eArgError, ":pointer argument is not a valid pointer");
    return NULL;
}

static inline char*
getString(VALUE value, int type)
{
    if (type == T_STRING) {

        if (rb_safe_level() >= 1 && OBJ_TAINTED(value)) {
            rb_raise(rb_eSecurityError, "Unsafe string parameter");
        }

        return StringValueCStr(value);

    } else if (type == T_NIL) {
        return NULL;
    }

    rb_raise(rb_eArgError, "Invalid String value");
}

static void*
callback_param(VALUE proc, VALUE cbInfo)
{
    VALUE callback ;
    if (proc == Qnil) {
        return NULL ;
    }

    // Handle Function pointers here
    if (rb_obj_is_kind_of(proc, rbffi_FunctionClass)) {
        AbstractMemory* ptr;
        Data_Get_Struct(proc, AbstractMemory, ptr);
        return ptr->address;
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

