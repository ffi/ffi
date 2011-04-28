require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

module LibTest
  extend FFI::Library
  ffi_lib LIBTEST_PATH

  attach_function :bench, :bench_PPP_v, [ :pointer, :pointer, :pointer ], :void
end


puts "Benchmark [ :pointer, :pointer, :pointer ], :void performance, #{ITER}x calls"
ptr = FFI::MemoryPointer.new :int
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench(ptr, ptr, ptr) }
  }
}
puts "Benchmark [ :pointer, :pointer, :pointer ], :void with Struct parameters performance #{ITER}x calls"

class TestStruct < FFI::Struct
  layout :i, :int
end

s = TestStruct.new(FFI::MemoryPointer.new(TestStruct));
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench(s, s, s) }
  }
}

puts "Benchmark [ :pointer, :pointer, :pointer ], :void with Buffer parameters performance, #{ITER}x calls"
ptr = FFI::Buffer.new(:int)
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench(ptr, ptr, ptr) }
  }
}

puts "Benchmark [ :pointer, :pointer, :pointer ], :void with nil parameters performance, #{ITER}x calls"
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench(nil, nil, nil) }
  }
}

