/*
 * Copyright (c) 2010 Wayne Meissner
 *
 * All rights reserved.
 *
 * This file is part of ruby-ffi.
 *
 * This code is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * version 3 for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with this work.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RBFFI_THREAD_H
#define	RBFFI_THREAD_H

#ifndef _MSC_VER
# include <stdbool.h>
#else
# include "win32/stdbool.h"
# include "win32/stdint.h"
#endif
#include <ruby.h>
#include "extconf.h"

#ifdef	__cplusplus
extern "C" {
#endif


#ifdef _WIN32
# include <windows.h>
#else
# include <pthread.h>
#endif

typedef struct {
#ifdef _WIN32
    DWORD id;
#else
    pthread_t id;
#endif
    bool valid;
    bool has_gvl;
    VALUE exc;
} rbffi_thread_t;

typedef struct rbffi_frame {
#ifndef _WIN32
    struct thread_data* td;
#endif
    struct rbffi_frame* prev;
    bool has_gvl;
    VALUE exc;
} rbffi_frame_t;

rbffi_frame_t* rbffi_frame_current(void);
void rbffi_frame_push(rbffi_frame_t* frame);
void rbffi_frame_pop(rbffi_frame_t* frame);

#ifdef HAVE_RB_THREAD_CALL_WITHOUT_GVL
# define rbffi_thread_blocking_region rb_thread_call_without_gvl

#elif defined(HAVE_RB_THREAD_BLOCKING_REGION)
# define rbffi_thread_blocking_region rb_thread_blocking_region

#else

VALUE rbffi_thread_blocking_region(VALUE (*func)(void *), void *data1, void (*ubf)(void *), void *data2);

#endif


#ifdef	__cplusplus
}
#endif

#endif	/* RBFFI_THREAD_H */

