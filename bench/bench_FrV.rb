require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

module LibTest
  extend FFI::Library
  ffi_lib LIBTEST_PATH
  attach_function :bench_f32_v, [ :float ], :void
end


puts "Benchmark [ :float ], :void performance, #{ITER}x calls"
f = 1.0
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench_f32_v(f) }
  }
}
puts "Benchmark Invoker.call [ :float  ], :void performance, #{ITER}x calls"

invoker = FFI.create_invoker(LIBTEST_PATH, 'bench_f32_v', [ :float ], :void)
10.times {
  puts Benchmark.measure {
    ITER.times { invoker.call1(f) }
  }
}
