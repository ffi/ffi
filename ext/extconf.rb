#!/usr/bin/env ruby
require 'mkmf'
require 'rbconfig'
dir_config("ffi_c")
libffi_ok = have_library("ffi", "ffi_prep_closure_loc", "ffi.h")
create_makefile("ffi_c")
File.open("Makefile", "a") do |mf|
  mf.puts "include $(srcdir)/ffi.mk"
  if Config::CONFIG['host_os'] =~ /darwin/
    mf.puts "include $(srcdir)/libffi.darwin.mk"
  else
    mf.puts "include $(srcdir)/libffi.mk"
  end
end unless libffi_ok
