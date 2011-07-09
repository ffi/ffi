require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

module LibTest
  extend FFI::Library
  ffi_lib LIBTEST_PATH

  attach_function :bench, :bench_P_v, [ :buffer_in ], :void
end


puts "Benchmark [ :buffer_in ], :void performance, #{ITER}x calls"
ptr = FFI::MemoryPointer.new :int
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench(ptr) }
  }
}

puts "Benchmark [ :buffer_in ], :void performance (nil param), #{ITER}x calls"
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench(nil) }
  }
}

puts "Benchmark [ :buffer_in ], :void performance (Buffer param), #{ITER}x calls"
ptr = FFI::Buffer.new :int
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench(ptr) }
  }
}
puts "Benchmark [ :buffer_in ], :void performance (const String param), #{ITER}x calls"
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench('test') }
  }
}
puts "Benchmark [ :buffer_in ], :void performance (loop-allocated Buffer param), #{ITER}x calls"
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench(FFI::Buffer.new(4)) }
  }
}
puts "Benchmark [ :buffer_in ], :void performance (loop-allocated String param), #{ITER}x calls"
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench(String.new) }
  }
}
puts "Benchmark [ :buffer_in ], :void performance (loop-allocated MemoryPointer param), #{ITER}x calls"
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench(FFI::MemoryPointer.new(4)) }
  }
}


