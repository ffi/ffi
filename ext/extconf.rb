#!/usr/bin/env ruby
require 'mkmf'
dir_config("ffi_c")
create_makefile("ffi_c")
File.open("Makefile", "a") do |mf|
  mf.puts "include $(srcdir)/ffi.mk"
end
