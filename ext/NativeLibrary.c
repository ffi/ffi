#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <ruby.h>

#include <ffi.h>

#include "rbffi.h"
#include "compat.h"
#include "Pointer.h"
#include "NativeLibrary.h"

static void library_mark(Library* lib);
static void library_free(Library* lib);
static VALUE classLibrary;

static VALUE
library_open(VALUE klass, VALUE libname, VALUE libflags)
{
    VALUE retval;
    Library* library;
    
    Check_Type(libflags, T_FIXNUM);

    retval = Data_Make_Struct(klass, Library, library_mark, library_free, library);
    library->handle = dlopen(libname != Qnil ? StringValueCStr(libname) : NULL, FIX2INT(libflags));
    if (library->handle == NULL) {
        rb_raise(rb_eLoadError, "Could not open library '%s': %s",
                libname != Qnil ? StringValueCStr(libname) : "[current process]",
                dlerror());
    }
    return retval;
}

static VALUE
library_dlsym(VALUE self, VALUE name)
{
    Library* library;
    void* address = NULL;
    Check_Type(name, T_STRING);

    Data_Get_Struct(self, Library, library);
    address = dlsym(library->handle, StringValueCStr(name));
    return address != NULL ? rb_FFI_Pointer_new(address) : Qnil;
}

static VALUE
library_dlerror(VALUE self)
{
    return rb_tainted_str_new2(dlerror());
}

static void
library_mark(Library* library)
{
}

static void
library_free(Library* library)
{
    if (library != NULL) {
        // dlclose() on MacOS tends to segfault - avoid it
#ifndef __APPLE__
        if (library->handle != NULL && library->handle != RTLD_DEFAULT) {
            dlclose(library->handle);
        }
#endif
        library->handle = NULL;
        xfree(library);
    }
}

void
rb_FFI_NativeLibrary_Init()
{
    VALUE moduleFFI = rb_define_module("FFI");
    classLibrary = rb_define_class_under(moduleFFI, "NativeLibrary", rb_cObject);
    rb_define_singleton_method(classLibrary, "open", library_open, 2);
    rb_define_method(classLibrary, "find_symbol", library_dlsym, 1);
    rb_define_method(classLibrary, "last_error", library_dlerror, 0);
}