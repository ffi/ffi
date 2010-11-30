require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))
METHOD = 'bench_s32s32s32s32s32s32_v'
module LibTest
  extend FFI::Library
  ffi_lib LIBTEST_PATH
  attach_function METHOD, [ :int, :int, :int, :int, :int, :int ], :void
  def self.rb_bench(i0, i1, i2, i3, i4, i5); nil; end
end


puts "Benchmark [ :int, :int, :int, :int, :int, :int ], :void performance, #{ITER}x calls"

10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench_s32s32s32s32s32s32_v(1, 2, 3, 4, 5, 6) }
  }
}
puts "Benchmark ruby method(6 arg)  performance, #{ITER}x calls"
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.rb_bench(0, 1, 2, 3, 4, 5) }
  }
}

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
    extern "void bench_s32s32s32s32s32s32_v(int, int, int, int, int, int)"
  end
  puts "Benchmark DL void bench(int, int, int) performance, #{ITER}x calls"
  10.times {
    puts Benchmark.measure {
      ITER.times { LibTest.bench_s32s32s32s32s32s32_v(0, 1, 2, 3, 4, 5) }
    }
  }
end

