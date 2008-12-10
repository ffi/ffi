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

#ifdef _WIN32
static void* dl_open(const char* name, int flags);
static void dl_error(char* buf, int size);
#define dl_sym(handle, name) GetProcAddress(handle, name)
#define dl_close(handle) FreeLibrary(handle)
enum { RTLD_LAZY=1, RTLD_NOW, RTLD_GLOBAL, RTLD_LOCAL };
#else
#define dl_open(name, flags) dlopen(name, flags != 0 ? flags : RTLD_LAZY)
#define dl_error(buf, size) do { snprintf(buf, size, "%s", dlerror()); } while(0)
#define dl_sym(handle, name) dlsym(handle, name)
#define dl_close(handle) dlclose(handle)
#endif

static VALUE
library_open(VALUE klass, VALUE libname, VALUE libflags)
{
    VALUE retval;
    Library* library;
    int flags;

    Check_Type(libflags, T_FIXNUM);

    retval = Data_Make_Struct(klass, Library, NULL, library_free, library);
    flags = libflags != Qnil ? NUM2UINT(libflags) : 0;
    
    library->handle = dl_open(libname != Qnil ? StringValueCStr(libname) : NULL, flags);
    if (library->handle == NULL) {
        char errmsg[1024];
        dl_error(errmsg, sizeof(errmsg));
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
    address = dl_sym(library->handle, StringValueCStr(name));
    return address != NULL ? rb_FFI_Pointer_new(address) : Qnil;
}

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
    if (library != NULL) {
        // dlclose() on MacOS tends to segfault - avoid it
#ifndef __APPLE__
        if (library->handle != NULL) {
            dl_close(library->handle);
        }
#endif
        library->handle = NULL;
        xfree(library);
    }
}

#ifdef _WIN32
static void*
dl_open(const char* name, int flags)
{
    return LoadLibraryEx(name, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
}

static void
dl_error(char* buf, int size)
{
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
            0, buf, size, NULL);
}
#endif

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

#define DEF(x) rb_define_const(classLibrary, "RTLD_" #x, UINT2NUM(RTLD_##x))
    DEF(LAZY);
    DEF(NOW);
    DEF(GLOBAL);
    DEF(LOCAL);

}

