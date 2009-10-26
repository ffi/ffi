#!/usr/bin/env ruby
require 'mkmf'
require 'rbconfig'
dir_config("ffi_c")

IS_MAC = Config::CONFIG['host_os'] =~ /^darwin/
pkg_config("libffi")
#have_libffi = find_library("ffi", "ffi_call", `pkg-config libffi --cflags --libs`)
have_ffi_call = have_library("ffi", "ffi_call", [ "ffi.h" ])
have_prep_closure = have_library("ffi", "ffi_prep_closure", [ "ffi.h" ])
libffi_ok = have_ffi_call && have_prep_closure
$defs << "-DHAVE_LIBFFI" if libffi_ok
$defs << "-DHAVE_RAW_API" if have_library("ffi", "ffi_raw_call", [ "ffi.h" ]) && have_library("ffi", "ffi_prep_raw_closure", [ "ffi.h"])


have_func('rb_thread_blocking_region')
$defs << "-DHAVE_EXTCONF_H" if $defs.empty? # needed so create_header works
create_makefile("ffi_c")
create_header
File.open("Makefile", "a") do |mf|
  mf.puts "CPPFLAGS += -Werror -Wunused -Wformat -Wimplicit -Wreturn-type"
  unless libffi_ok 
    mf.puts "LIBFFI_HOST=--host=#{Config::CONFIG['host_alias']}" if Config::CONFIG.has_key?("host_alias")
    if Config::CONFIG['host_os'].downcase =~ /darwin/
      mf.puts "include ${srcdir}/libffi.darwin.mk"
    elsif Config::CONFIG['host_os'].downcase =~ /bsd/
      mf.puts '.include "${srcdir}/libffi.bsd.mk"'
    else
      mf.puts "include ${srcdir}/libffi.mk"
    end
  end
end
