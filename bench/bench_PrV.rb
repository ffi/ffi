require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

module LibTest
  extend FFI::Library
  ffi_lib LIBTEST_PATH

  attach_function :bench, :bench_p_v, [ :pointer ], :void
end


puts "Benchmark [ :int ], :void performance, #{ITER}x calls"
ptr = FFI::MemoryPointer.new :int
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench(ptr) }
  }
}
puts "Benchmark Invoker.call [ :int  ], :void performance, #{ITER}x calls"

invoker = FFI.create_invoker(LIBTEST_PATH, 'bench_p_v', [ :pointer ], :void)
10.times {
  puts Benchmark.measure {
    ITER.times { invoker.call1(ptr) }
  }
}
