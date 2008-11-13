#!/usr/bin/env ruby
require 'mkmf'
require 'rbconfig'
dir_config("ffi_c")
IS_MAC = Config::CONFIG['host_os'] =~ /^darwin/
if IS_MAC
  $CPPFLAGS << " -DMACOSX"
  find_header("ffi.h", "/usr/include/ffi")
end
have_closure_alloc = have_library("ffi", "ffi_closure_alloc", [ "ffi.h" ])
$defs.push("-DHAVE_FFI_CLOSURE_ALLOC") if have_closure_alloc
libffi_ok = have_closure_alloc || (IS_MAC && have_library("ffi", "ffi_prep_closure", [ "ffi.h" ]))
$defs << "-DHAVE_LIBFFI" if libffi_ok

create_makefile("ffi_c")
create_header
File.open("Makefile", "a") do |mf|
  mf.puts "include $(srcdir)/ffi.mk"
  if Config::CONFIG['host_os'] =~ /darwin/
    mf.puts "include $(srcdir)/libffi.darwin.mk"
  else
    mf.puts "include $(srcdir)/libffi.mk"
  end
end unless libffi_ok
