/*
 * Copyright (c) 2008-2010 Wayne Meissner
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

#include <sys/types.h>
#include <stdio.h>
#ifndef _MSC_VER
#  include <stdint.h>
#endif
#if (defined(_WIN32) || defined(__WIN32__)) && !defined(__CYGWIN__)
# include <winsock2.h>
# define _WINSOCKAPI_
# include <windows.h>
#else
# include <dlfcn.h>
#endif
#include <ruby.h>
#if defined(_MSC_VER) && !defined(INT8_MIN)
#  include "win32/stdint.h"
#endif

#include <ffi.h>

#include "rbffi.h"
#include "compat.h"
#include "AbstractMemory.h"
#include "Pointer.h"
#include "DynamicLibrary.h"

typedef struct LibrarySymbol_ {
    Pointer base;
    VALUE library;
    VALUE name;
} LibrarySymbol;

static VALUE library_initialize(VALUE self, VALUE libname, VALUE libflags);
static void library_free(Library* lib);


static VALUE symbol_allocate(VALUE klass);
static VALUE symbol_new(VALUE library, void* address, VALUE name);
static void symbol_mark(LibrarySymbol* sym);

static VALUE LibraryClass = Qnil, SymbolClass = Qnil;

#if (defined(_WIN32) || defined(__WIN32__)) && !defined(__CYGWIN__)
static void* dl_open(const char* name, int flags);
static void dl_error(char* buf, int size);
#define dl_sym(handle, name) GetProcAddress(handle, name)
#define dl_close(handle) FreeLibrary(handle)
enum { RTLD_LAZY=1, RTLD_NOW, RTLD_GLOBAL, RTLD_LOCAL };
#else
# define dl_open(name, flags) dlopen(name, flags != 0 ? flags : RTLD_LAZY)
# define dl_error(buf, size) do { snprintf(buf, size, "%s", dlerror()); } while(0)
# define dl_sym(handle, name) dlsym(handle, name)
# define dl_close(handle) dlclose(handle)
#ifndef RTLD_LOCAL
# define RTLD_LOCAL 8
#endif
#endif

static VALUE
library_allocate(VALUE klass)
{
    Library* library;
    return Data_Make_Struct(klass, Library, NULL, library_free, library);
}

/*
 * call-seq: DynamicLibrary.open(libname, libflags)
 * @param libname (see #initialize)
 * @param libflags (see #initialize)
 * @return [FFI::DynamicLibrary]
 * @raise {LoadError} if +libname+ cannot be opened
 * Open a library.
 */
static VALUE
library_open(VALUE klass, VALUE libname, VALUE libflags)
{
    return library_initialize(library_allocate(klass), libname, libflags);
}

/*
 * call-seq: initialize(libname, libflags)
 * @param [String] libname name of library to open
 * @param [Fixnum] libflags flags for library to open
 * @return [FFI::DynamicLibrary]
 * @raise {LoadError} if +libname+ cannot be opened
 * A new DynamicLibrary instance.
 */
static VALUE
library_initialize(VALUE self, VALUE libname, VALUE libflags)
{
    Library* library;
    int flags;

    Check_Type(libflags, T_FIXNUM);

    Data_Get_Struct(self, Library, library);
    flags = libflags != Qnil ? NUM2UINT(libflags) : 0;
    
    library->handle = dl_open(libname != Qnil ? StringValueCStr(libname) : NULL, flags);
    if (library->handle == NULL) {
        char errmsg[1024];
        dl_error(errmsg, sizeof(errmsg));
        rb_raise(rb_eLoadError, "Could not open library '%s': %s",
                libname != Qnil ? StringValueCStr(libname) : "[current process]",
                errmsg);
    }
#ifdef __CYGWIN__
    // On Cygwin 1.7.17 "dlsym(dlopen(0,0), 'getpid')" fails. (dlerror: "No such process")
    // As a workaround we can use "dlsym(RTLD_DEFAULT, 'getpid')" instead.
    // Since 0 == RTLD_DEFAULT we won't call dl_close later.
    if (libname == Qnil) {
        dl_close(library->handle);
        library->handle = RTLD_DEFAULT;
    }
#endif
    rb_iv_set(self, "@name", libname != Qnil ? libname : rb_str_new2("[current process]"));
    return self;
}

static VALUE
library_dlsym(VALUE self, VALUE name)
{
    Library* library;
    void* address = NULL;
    Check_Type(name, T_STRING);

    Data_Get_Struct(self, Library, library);
    address = dl_sym(library->handle, StringValueCStr(name));
    
    return address != NULL ? symbol_new(self, address, name) : Qnil;
}

/*
 * call-seq: last_error
 * @return [String] library's last error string
 */
static VALUE
library_dlerror(VALUE self)
{
    char errmsg[1024];
    dl_error(errmsg, sizeof(errmsg));
    return rb_tainted_str_new2(errmsg);
}

static void
library_free(Library* library)
{
    /* dlclose() on MacOS tends to segfault - avoid it */
#ifndef __APPLE__
    if (library->handle != NULL) {
        dl_close(library->handle);
    }
#endif
    xfree(library);
}

#if (defined(_WIN32) || defined(__WIN32__)) && !defined(__CYGWIN__)
static void*
dl_open(const char* name, int flags)
{
    if (name == NULL) {
        return GetModuleHandle(NULL);
    } else {
        return LoadLibraryExA(name, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
}

static void
dl_error(char* buf, int size)
{
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
            0, buf, size, NULL);
}
#endif

static VALUE
symbol_allocate(VALUE klass)
{
    LibrarySymbol* sym;
    VALUE obj = Data_Make_Struct(klass, LibrarySymbol, NULL, -1, sym);
    sym->name = Qnil;
    sym->library = Qnil;
    sym->base.rbParent = Qnil;

    return obj;
}


/*
 * call-seq: initialize_copy(other)
 * @param [Object] other
 * @return [nil]
 * DO NOT CALL THIS METHOD
 */
static VALUE
symbol_initialize_copy(VALUE self, VALUE other)
{
    rb_raise(rb_eRuntimeError, "cannot duplicate symbol");
    return Qnil;
}

static VALUE
symbol_new(VALUE library, void* address, VALUE name)
{
    LibrarySymbol* sym;
    VALUE obj = Data_Make_Struct(SymbolClass, LibrarySymbol, symbol_mark, -1, sym);

    sym->base.memory.address = address;
    sym->base.memory.size = LONG_MAX;
    sym->base.memory.typeSize = 1;
    sym->base.memory.flags = MEM_RD | MEM_WR;
    sym->library = library;
    sym->name = name;

    return obj;
}

static void
symbol_mark(LibrarySymbol* sym)
{
    rb_gc_mark(sym->library);
    rb_gc_mark(sym->name);
}

/*
 * call-seq: inspect
 * @return [String]
 * Inspect.
 */
static VALUE
symbol_inspect(VALUE self)
{
    LibrarySymbol* sym;
    char buf[256];

    Data_Get_Struct(self, LibrarySymbol, sym);
    snprintf(buf, sizeof(buf), "#<FFI::Library::Symbol name=%s address=%p>",
             StringValueCStr(sym->name), sym->base.memory.address);
    return rb_str_new2(buf);
}

void
rbffi_DynamicLibrary_Init(VALUE moduleFFI)
{
    /*
     * Document-class: FFI::DynamicLibrary
     */
    LibraryClass = rb_define_class_under(moduleFFI, "DynamicLibrary", rb_cObject);
    rb_global_variable(&LibraryClass);
    /*
     * Document-class: FFI::DynamicLibrary::Symbol < FFI::Pointer
     *
     * An instance of this class represents a library symbol. It may be a {Pointer pointer} to
     * a function or to a variable.
     */
    SymbolClass = rb_define_class_under(LibraryClass, "Symbol", rbffi_PointerClass);
    rb_global_variable(&SymbolClass);

    /*
     * Document-const: FFI::NativeLibrary
     * Backward compatibility for FFI::DynamicLibrary
     */
    rb_define_const(moduleFFI, "NativeLibrary", LibraryClass); /* backwards compat library */
    rb_define_alloc_func(LibraryClass, library_allocate);
    rb_define_singleton_method(LibraryClass, "open", library_open, 2);
    rb_define_singleton_method(LibraryClass, "last_error", library_dlerror, 0);
    rb_define_method(LibraryClass, "initialize", library_initialize, 2);
    /*
     * Document-method: find_symbol
     * call-seq: find_symbol(name)
     * @param [String] name library symbol's name
     * @return [FFI::DynamicLibrary::Symbol] library symbol
     */
    rb_define_method(LibraryClass, "find_symbol", library_dlsym, 1);
    /*
     * Document-method: find_function
     * call-seq: find_function(name)
     * @param [String] name library function's name
     * @return [FFI::DynamicLibrary::Symbol] library function symbol
     */
    rb_define_method(LibraryClass, "find_function", library_dlsym, 1);
    /*
     * Document-method: find_variable
     * call-seq: find_variable(name)
     * @param [String] name library variable's name
     * @return [FFI::DynamicLibrary::Symbol] library variable symbol
     */
    rb_define_method(LibraryClass, "find_variable", library_dlsym, 1);
    rb_define_method(LibraryClass, "last_error", library_dlerror, 0);
    rb_define_attr(LibraryClass, "name", 1, 0);

    rb_define_alloc_func(SymbolClass, symbol_allocate);
    rb_undef_method(SymbolClass, "new");
    rb_define_method(SymbolClass, "inspect", symbol_inspect, 0);
    rb_define_method(SymbolClass, "initialize_copy", symbol_initialize_copy, 1);
    

#define DEF(x) rb_define_const(LibraryClass, "RTLD_" #x, UINT2NUM(RTLD_##x))
    DEF(LAZY);
    DEF(NOW);
    DEF(GLOBAL);
    DEF(LOCAL);

}

