require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))
METHOD = 'bench_s32s32s32s32s32s32_v'
module LibTest
  extend FFI::Library
  ffi_lib LIBTEST_PATH
  attach_function METHOD, [ :int, :int, :int, :int, :int, :int ], :void
end


puts "Benchmark [ :int, :int, :int, :int, :int, :int ], :void performance, #{ITER}x calls"

10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench_s32s32s32s32s32s32_v(1, 2, 3, 4, 5, 6) }
  }
}

unless RUBY_PLATFORM =~ /java/
puts "Benchmark Invoker.call [ :int, :int, :int, :int, :int, :int ], :void performance, #{ITER}x calls"

invoker = FFI.create_invoker(LIBTEST_PATH, METHOD.to_s, [ :int, :int, :int, :int, :int, :int ], :void)
10.times {
  puts Benchmark.measure {
    ITER.times { invoker.call(1, 2, 3, 4, 5, 6) }
  }
} 
end
