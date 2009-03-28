#!/usr/bin/env ruby
require 'mkmf'
require 'rbconfig'
dir_config("ffi_c")
#IS_MAC = Config::CONFIG['host_os'] =~ /^darwin/
#if IS_MAC
#  $CPPFLAGS << " -DMACOSX"
#  find_header("ffi.h", "/usr/include/ffi")
#end
have_closure_alloc = have_library("ffi", "ffi_closure_alloc", [ "ffi.h" ])
$defs.push("-DHAVE_FFI_CLOSURE_ALLOC") if have_closure_alloc
libffi_ok = have_closure_alloc
$defs << "-DHAVE_LIBFFI" if libffi_ok
$defs << "-DHAVE_EXTCONF_H" if $defs.empty? # needed so create_header works

create_makefile("ffi_c")
create_header("extconf.h")
File.open("Makefile", "a") do |mf|
  mf.puts "CPPFLAGS += -Werror -Wunused -Wformat -Wimplicit"
  unless libffi_ok 
    mf.puts "include $(srcdir)/ffi.mk"
    mf.puts "LIBFFI_HOST=--host=#{Config::CONFIG['host_alias']}" if Config::CONFIG.has_key?("host_alias")
    mf.puts "FFI_MMAP_EXEC=-DFFI_MMAP_EXEC_WRIT=#{Config::CONFIG['host_os'] =~ /win/ ? 0 : 1}"
    if Config::CONFIG['host_os'] =~ /darwin/
      mf.puts "include $(srcdir)/libffi.darwin.mk"
    else
      mf.puts "include $(srcdir)/libffi.mk"
    end
  end
end
