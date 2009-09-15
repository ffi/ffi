require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

module LibTest
  extend FFI::Library
  ffi_lib LIBTEST_PATH
  attach_function :ffi_bench, :bench_s32s32s32_v, [ :int, :int, :int ], :void
  def self.rb_bench(i0, i1, i2); nil; end
end

unless RUBY_PLATFORM == "java" && JRUBY_VERSION < "1.3.0"
  require 'dl'
  require 'dl/import'
  module LibTest
    if RUBY_VERSION >= "1.9.0"
      extend DL::Importer
    else
      extend DL::Importable
    end
    dlload LIBTEST_PATH
    extern "void bench_s32s32s32_v(int)"
  end
end


puts "Benchmark [ :int, :int, :int ], :void performance, #{ITER}x calls"
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.ffi_bench(0, 1, 2) }
  }
}
puts "Benchmark Invoker.call [ :int, :int, :int ], :void performance, #{ITER}x calls"

invoker = FFI.create_invoker(LIBTEST_PATH, 'bench_s32s32s32_v', [ :int, :int, :int ], :void)
10.times {
  puts Benchmark.measure {
    ITER.times { invoker.call(0, 1, 2) }
  }
}
puts "Benchmark ruby method(3 arg)  performance, #{ITER}x calls"
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.rb_bench(0, 1, 2) }
  }
}
unless RUBY_PLATFORM == "java" && JRUBY_VERSION < "1.3.0"
puts "Benchmark DL void bench(int, int, int) performance, #{ITER}x calls"
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench_s32s32s32_v(0, 1, 2) }
  }
}
end

