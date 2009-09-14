#!/usr/bin/env ruby
require 'mkmf'
require 'rbconfig'
dir_config("ffi_c")

IS_MAC = Config::CONFIG['host_os'] =~ /^darwin/
have_closure_alloc = have_library("ffi", "ffi_closure_alloc", [ "ffi.h" ])
$defs.push("-DHAVE_FFI_CLOSURE_ALLOC") if have_closure_alloc
libffi_ok = have_closure_alloc && !IS_MAC
$defs << "-DHAVE_LIBFFI" if libffi_ok
$defs << "-DHAVE_EXTCONF_H" if $defs.empty? # needed so create_header works

have_func('rb_thread_blocking_region')

create_makefile("ffi_c")
create_header("extconf.h")
File.open("Makefile", "a") do |mf|
  mf.puts "CPPFLAGS += -Werror -Wunused -Wformat -Wimplicit -Wreturn-type"
  unless libffi_ok 
    mf.puts "LIBFFI_HOST=--host=#{Config::CONFIG['host_alias']}" if Config::CONFIG.has_key?("host_alias")
    mf.puts "FFI_MMAP_EXEC=-DFFI_MMAP_EXEC_WRIT=#{Config::CONFIG['host_os'] =~ /(win32|mingw)/ ? 0 : 1}"
    if Config::CONFIG['host_os'].downcase =~ /darwin/
      mf.puts "include ${srcdir}/libffi.darwin.mk"
    elsif Config::CONFIG['host_os'].downcase =~ /bsd/
      mf.puts '.include "${srcdir}/libffi.bsd.mk"'
    else
      mf.puts "include ${srcdir}/libffi.mk"
    end
  end
end
