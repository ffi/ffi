#!/usr/bin/env ruby
#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#

# This test specifically avoids calling native code through FFI.
# Instead, the stock extension mechanism is used. The reason is
# that the C extension initializes FFI and then calls a callback
# which deadlocked in earlier FFI versions, see
# https://github.com/ffi/ffi/issues/527

EXT = File.expand_path("ext/embed_test.so", File.dirname(__FILE__))

old = Dir.pwd
Dir.chdir(File.dirname(EXT))

nul = File.open(File::NULL)
make = system('type gmake', { :out => nul, :err => nul }) && 'gmake' || 'make'

# create Makefile
system(RbConfig.ruby, "extconf.rb")

# compile extension
unless system(make)
  raise "Unable to compile \"#{EXT}\""
end

Dir.chdir(old)

require EXT
EmbedTest::testfunc
