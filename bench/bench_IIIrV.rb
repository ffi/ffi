require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

module LibTest
  extend FFI::Library
  ffi_lib LIBTEST_PATH
  attach_function :benchIIIrV, [ :int, :int, :int ], :void
end


puts "Benchmark [ :int, :int, :int ], :void performance, #{ITER}x calls"

10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.benchIIIrV(0, 1, 2) }
  }
}
puts "Benchmark Invoker.call [ :int, :int, :int ], :void performance, #{ITER}x calls"

invoker = FFI.create_invoker(LIBTEST_PATH, 'benchIIIrV', [ :int, :int, :int ], :void)
# Patch Invoker to have call3 on older FFI versions
unless invoker.respond_to?("call3")
  class FFI::Invoker
    alias :call3 :call
  end
end
10.times {
  puts Benchmark.measure {
    ITER.times { invoker.call3(0, 1, 2) }
  }
}
