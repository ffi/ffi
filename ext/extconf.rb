#!/usr/bin/env ruby
require 'mkmf'
dir_config("ffi_c")
create_makefile("ffi_c")
File.open("Makefile", "a") do |mf|
  mf.puts "OS=$(shell uname -s)"
  mf.puts "ARCH=$(shell uname -p)"
  mf.puts 'CFLAGS += -DOS=\\"$(OS)\\" -DARCH=\\"$(ARCH)\\"'
  mf.puts "include $(srcdir)/libffi.mk"
  mf.puts "$(OBJS):\t$(LIBFFI)"
  mf.puts "LOCAL_LIBS += $(LIBFFI)"
  mf.puts "clean:\tlibffi_clean"
end
