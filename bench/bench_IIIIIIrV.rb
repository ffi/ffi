require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

module LibTest
  extend FFI::Library
  ffi_lib LIBTEST_PATH
  attach_function :benchIIIIIIrV, [ :int, :int, :int, :int, :int, :int ], :void
end


puts "Benchmark [ :int, :int, :int, :int, :int, :int ], :void performance, #{ITER}x calls"

10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.benchIIIIIIrV(1, 2, 3, 4, 5, 6) }
  }
}

puts "Benchmark Invoker.call [ :int, :int, :int, :int, :int, :int ], :void performance, #{ITER}x calls"

invoker = FFI.create_invoker(LIBTEST_PATH, 'benchIIIIIIrV', [ :int, :int, :int, :int, :int, :int ], :void)
10.times {
  puts Benchmark.measure {
    ITER.times { invoker.call(1, 2, 3, 4, 5, 6) }
  }
}

