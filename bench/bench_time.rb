require_relative 'bench_helper'

module BenchTime
  module Posix
    extend FFI::Library
    ffi_lib FFI::Library::LIBC
    if FFI::Library::LIBC =~ /ucrtbase/
      attach_function :time, :_time64, [ :buffer_out ], :uint64, ignore_error: true
    else
      attach_function :time, [ :buffer_out ], :ulong, ignore_error: true
    end
  end

  puts "Benchmark FFI time(3) with nil argument performance, #{ITER}x"
  10.times {
    puts Benchmark.measure {
      ITER.times { Posix.time(nil) }
    }
  }

  puts "Benchmark FFI time(3) with pre-allocated buffer performance, #{ITER}x"
  buf = FFI::Buffer.new(:time_t)
  10.times {
    puts Benchmark.measure {
      ITER.times { Posix.time(buf) }
    }
  }


  puts "Benchmark FFI time(3) with loop-allocated buffer performance, #{ITER}x"
  10.times {
    puts Benchmark.measure {
      ITER.times { Posix.time(FFI::Buffer.new(:time_t)) }
    }
  }

  puts "Benchmark FFI time(3) with pre-allocated pointer performance, #{ITER}x"
  buf = FFI::MemoryPointer.new(:time_t)
  10.times {
    puts Benchmark.measure {
      ITER.times { Posix.time(buf) }
    }
  }

  puts "Benchmark Time.now performance, #{ITER}x"
  10.times {
    puts Benchmark.measure {
      ITER.times { Time.now }
    }
  }
end
