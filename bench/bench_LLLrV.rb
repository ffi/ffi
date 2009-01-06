require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

module LibTest
  extend FFI::Library
  ffi_lib LIBTEST_PATH
  attach_function :bench_s64s64s64_v, [ :long_long, :long_long, :long_long ], :void
end


puts "Benchmark [ :long_long, :long_long, :long_long ], :void performance, #{ITER}x calls"

10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench_s64s64s64_v(0, 1, 2) }
  }
}
unless RUBY_PLATFORM =~ /java/
puts "Benchmark Invoker.call [ :long_long, :long_long, :long_long ], :void performance, #{ITER}x calls"

invoker = FFI.create_invoker(LIBTEST_PATH, 'bench_s64s64s64_v', [ :long_long, :long_long, :long_long ], :void)
10.times {
  puts Benchmark.measure {
    ITER.times { invoker.call3(0, 1, 2) }
  }
}
end
