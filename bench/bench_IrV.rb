require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

module LibTest
  extend FFI::Library
  ffi_lib LIBTEST_PATH
  attach_function :bench_s32_v, [ :int ], :void
end


puts "Benchmark [ :int ], :void performance, #{ITER}x calls"

10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench_s32_v(0) }
  }
}
puts "Benchmark Invoker.call [ :int  ], :void performance, #{ITER}x calls"

invoker = FFI.create_invoker(LIBTEST_PATH, 'bench_s32_v', [ :int ], :void)
unless invoker.respond_to?("call1")
  class FFI::Invoker
    alias :call1 :call
  end
end
10.times {
  puts Benchmark.measure {
    ITER.times { invoker.call1(1) }
  }
}
