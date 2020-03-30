require_relative 'bench_helper'
require 'etc'

module BenchGetlogin
  iter = ITER

  module Posix
    extend FFI::Library
    ffi_lib FFI::Library::LIBC
    attach_function :getlogin, [], :string
  end

  # getlogin(2) might return NULL, in which case Etc.getlogin uses ENV["USER"].
  # This should not matter for this benchmark, getlogin() is still called.
  if (Posix.getlogin || ENV["USER"]) != Etc.getlogin
    raise ArgumentError, "FFI getlogin returned incorrect value: " \
      "#{Posix.getlogin.inspect} (FFI) vs #{Etc.getlogin.inspect} (Etc)"
  end

  puts "Benchmark FFI getlogin(2) performance, #{ITER}x"

  10.times {
    puts Benchmark.measure {
      iter.times { Posix.getlogin }
    }
  }

  puts "Benchmark Etc.getlogin performance, #{ITER}x"
  10.times {
    puts Benchmark.measure {
      iter.times { Etc.getlogin }
    }
  }
end unless FFI::Platform.windows?
