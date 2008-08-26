#!/usr/bin/env ruby
require 'mkmf'
dir_config("ffi")
create_makefile("ffi")
File.open("Makefile", "a") do |mf|
  mf.puts "build_libffi:\n"
  mf.puts "\t$(MAKE) -f Makefile.libffi"
  mf.puts "$(DLLIB):\tbuild_libffi"
end
