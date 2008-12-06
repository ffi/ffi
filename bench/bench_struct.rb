require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

require 'benchmark'
require 'ffi'
iter = 100_000

module StructBench
  extend FFI::Library
  extend FFI::Library
  ffi_lib LIBTEST_PATH
  attach_function :bench_s32_v, [ :int ], :void
end
class IntStruct < FFI::Struct
  layout :v, :int
end
s = IntStruct.new
puts "Benchmark FFI Struct.get(:int) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { s[:v] }
  }
}
puts "Benchmark FFI Struct.put(:int) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { s[:v] = 0x12345678 }
  }
}
