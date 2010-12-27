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

#include <pthread.h>
#include "Thread.h"

#ifndef HAVE_RUBY_THREAD_HAS_GVL_P
rbffi_thread_t rbffi_active_thread;

rbffi_thread_t
rbffi_thread_self()
{
    rbffi_thread_t self;
    self.id = pthread_self();
    self.valid = true;

    return self;
}

bool
rbffi_thread_equal(rbffi_thread_t* lhs, rbffi_thread_t* rhs)
{
    return lhs->valid && rhs->valid && pthread_equal(lhs->id, rhs->id);
}

bool
rbffi_thread_has_gvl_p(void)
{
    return rbffi_active_thread.valid && pthread_equal(rbffi_active_thread.id, pthread_self());
}

#endif // HAVE_NATIVETHREAD