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

#include <stdbool.h>
#include <ruby.h>
#include "extconf.h"

#ifdef	__cplusplus
extern "C" {
#endif


#ifdef HAVE_RUBY_THREAD_HAS_GVL_P
# define rbffi_thread_has_gvl_p ruby_thread_has_gvl_p
#else

#include <pthread.h>

typedef struct {
    pthread_t id;
    bool valid;
} rbffi_thread_t;

extern rbffi_thread_t rbffi_active_thread;
rbffi_thread_t rbffi_thread_self();
bool rbffi_thread_equal(rbffi_thread_t* lhs, rbffi_thread_t* rhs);
bool rbffi_thread_has_gvl_p(void);

#endif
#ifdef HAVE_RB_THREAD_BLOCKING_REGION
# define rbffi_thread_blocking_region rb_thread_blocking_region
#else

VALUE rbffi_thread_blocking_region(VALUE (*func)(void *), void *data1, void (*ubf)(void *), void *data2);

#endif


#ifdef	__cplusplus
}
#endif

#endif	/* RBFFI_THREAD_H */

