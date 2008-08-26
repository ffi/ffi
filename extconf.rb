#!/usr/bin/env ruby
require 'mkmf'
dir_config("ffi")
create_makefile("ffi")
File.open("Makefile", "a") do |mf|
  mf.puts "libffi_build:"
  mf.puts "\t$(MAKE) -f libffi.mk"
  mf.puts "$(DLLIB):\tlibffi_build"
  mf.puts "libffi_clean:"
  mf.puts "\t$(MAKE) -f libffi.mk clean"
  mf.puts "clean:\tlibffi_clean"
end
