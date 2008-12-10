#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#ifdef _WIN32
# include <windows.h>
#else
# include <dlfcn.h>
#endif
#include <ruby.h>

#include <ffi.h>

#include "rbffi.h"
#include "compat.h"
#include "Pointer.h"
#include "NativeLibrary.h"

static void library_free(Library* lib);
static VALUE classLibrary;

static VALUE
library_open(VALUE klass, VALUE libname, VALUE libflags)
{
    VALUE retval;
    Library* library;
    int flags;

    Check_Type(libflags, T_FIXNUM);

    retval = Data_Make_Struct(klass, Library, NULL, library_free, library);
#ifdef _WIN32
    library->handle = LoadLibraryEx(StringValueCStr(libname), NULL,
            LOAD_WITH_ALTERED_SEARCH_PATH);
#else
    flags = libflags != Qnil ? NUM2UINT(libflags) : 0;
    if (flags == 0) {
        flags = RTLD_LAZY;
    }

    library->handle = dlopen(libname != Qnil ? StringValueCStr(libname) : NULL, flags);
#endif
    if (library->handle == NULL) {
        char errmsg[1024];
#ifdef _WIN32
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
                0, errmsg, sizeof(errmsg), NULL);
#else
        snprintf(errmsg, sizeof(errmsg), "%s", dlerror());
#endif
        rb_raise(rb_eLoadError, "Could not open library '%s': %s",
                libname != Qnil ? StringValueCStr(libname) : "[current process]",
                errmsg);
    }
    rb_iv_set(retval, "@name", libname != Qnil ? libname : rb_str_new2("[current process]"));
    return retval;
}

static VALUE
library_dlsym(VALUE self, VALUE name)
{
    Library* library;
    void* address = NULL;
    Check_Type(name, T_STRING);

    Data_Get_Struct(self, Library, library);
#ifdef _WIN32
    address = GetProcAddress(library->handle, StringValueCStr(name));
#else
    address = dlsym(library->handle, StringValueCStr(name));
#endif
    return address != NULL ? rb_FFI_Pointer_new(address) : Qnil;
}

static VALUE
library_dlerror(VALUE self)
{
#ifdef WIN32
    char errmsg[1024];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
            0, errmsg, sizeof(errmsg), NULL);
    return rb_tainted_str_new2(errmsg);
#else
    return rb_tainted_str_new2(dlerror());
#endif
}

static void
library_free(Library* library)
{
    if (library != NULL) {
        // dlclose() on MacOS tends to segfault - avoid it
#ifndef __APPLE__
        if (library->handle != NULL) {
# ifdef _WIN32
            FreeLibrary(library->handle);
# else
            dlclose(library->handle);
# endif
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
    classLibrary = rb_define_class_under(moduleFFI, "DynamicLibrary", rb_cObject);
    rb_define_const(moduleFFI, "NativeLibrary", classLibrary); // backwards compat library
    rb_define_singleton_method(classLibrary, "open", library_open, 2);
    rb_define_method(classLibrary, "find_symbol", library_dlsym, 1);
    rb_define_method(classLibrary, "last_error", library_dlerror, 0);
    rb_define_attr(classLibrary, "name", 1, 0);
#ifndef _WIN32
#define DEF(x) rb_define_const(classLibrary, "RTLD_" #x, UINT2NUM(RTLD_##x))
    DEF(LAZY);
    DEF(NOW);
    DEF(GLOBAL);
    DEF(LOCAL);
#endif // _WIN32
}

