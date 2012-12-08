require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

module LibTest
  extend FFI::Library
  ffi_lib LIBTEST_PATH

  attach_function :bench, :bench_PPP_v, [ :pointer, :pointer, :pointer ], :void, :save_errno => false
end


puts "Benchmark [ :pointer, :pointer, :pointer ], :void performance, #{ITER}x calls"
ptr = FFI::MemoryPointer.new :int
10.times {
  puts Benchmark.measure {
    i = 0; while i < ITER
      LibTest.bench(ptr, ptr, ptr)
      LibTest.bench(ptr, ptr, ptr)
      LibTest.bench(ptr, ptr, ptr)
      LibTest.bench(ptr, ptr, ptr)
      i += 4
    end
  }
}
puts "Benchmark [ :pointer, :pointer, :pointer ], :void with Struct parameters performance #{ITER}x calls"

class TestStruct < FFI::Struct
  layout :i, :int
end

s = TestStruct.new(FFI::MemoryPointer.new(TestStruct));
10.times {
  puts Benchmark.measure {
    i = 0; while i < ITER
      LibTest.bench(s, s, s)
      LibTest.bench(s, s, s)
      LibTest.bench(s, s, s)
      LibTest.bench(s, s, s)
      i += 4
    end
  }
}

puts "Benchmark [ :pointer, :pointer, :pointer ], :void with Buffer parameters performance, #{ITER}x calls"
ptr = FFI::Buffer.new(:int)
10.times {
  puts Benchmark.measure {
    i = 0; while i < ITER
      LibTest.bench(ptr, ptr, ptr)
      LibTest.bench(ptr, ptr, ptr)
      LibTest.bench(ptr, ptr, ptr)
      LibTest.bench(ptr, ptr, ptr)
      i += 4
    end
  }
}

puts "Benchmark [ :pointer, :pointer, :pointer ], :void with nil parameters performance, #{ITER}x calls"
10.times {
  puts Benchmark.measure {
    i = 0; while i < ITER
      LibTest.bench(nil, nil, nil)
      LibTest.bench(nil, nil, nil)
      LibTest.bench(nil, nil, nil)
      LibTest.bench(nil, nil, nil)
      i += 4
    end
  }
}

