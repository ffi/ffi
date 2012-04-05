require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

require 'benchmark'
require 'ffi'
iter = ITER

puts "Benchmark MemoryPointer.new(:int, 1, true)) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { FFI::MemoryPointer.new(:int, 1, true) }
  }
}
puts "Benchmark MemoryPointer.new(4, 1, true)) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { FFI::MemoryPointer.new(4, 1, true) }
  }
}
[ 8, 16, 32, 64, 128, 256 ].each do |size|
puts "Benchmark MemoryPointer.new(#{size}, 1, true)) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { FFI::MemoryPointer.new(size, 1, true) }
  }
}
end

if defined?(RUBY_ENGINE) && RUBY_ENGINE == "jruby"
  require 'java'
  puts "calling java gc"
  10.times { 
    java.lang.System.gc 
    sleep 1
  }
end

