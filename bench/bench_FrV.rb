require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

module LibTest
  extend FFI::Library
  ffi_lib LIBTEST_PATH
  attach_function :bench_f32_v, [ :float ], :void
  def self.rb_bench(i0); nil; end
end


puts "Benchmark [ :float ], :void performance, #{ITER}x calls"
f = 1.0
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.bench_f32_v(f) }
  }
}

puts "Benchmark ruby method(1 arg)  performance, #{ITER}x calls"
10.times {
  puts Benchmark.measure {
    ITER.times { LibTest.rb_bench(f) }
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
    extern "void bench_f32_v(float)"
  end
  puts "Benchmark DL void bench(float) performance, #{ITER}x calls"
  10.times {
    puts Benchmark.measure {
      ITER.times { LibTest.bench_f32_v(0) }
    }
  }
end

