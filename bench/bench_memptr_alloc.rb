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

