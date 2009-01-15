require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

require 'benchmark'
require 'ffi'
iter = 100_000

module StructBench
  extend FFI::Library
  extend FFI::Library
  ffi_lib LIBTEST_PATH
  attach_function :bench_s32_v, [ :int ], :void
  attach_function :bench_struct_in, :ptr_ret_int32_t, [ :buffer_in, :int ], :void
  attach_function :bench_struct_out, :ptr_ret_int32_t, [ :buffer_out, :int ], :void
  attach_function :bench_struct_inout, :ptr_ret_int32_t, [ :buffer_inout, :int ], :void
end
class TestStruct < FFI::Struct
  layout :i, :int, :p, :pointer
end
puts "Benchmark FFI call(Struct.alloc_in) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { StructBench.bench_struct_in(TestStruct.alloc_in, 0) }
  }
}
puts "Benchmark FFI call(Struct.alloc_out) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { StructBench.bench_struct_out(TestStruct.alloc_out, 0) }
  }
}
puts "Benchmark FFI call(Struct.alloc_inout) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { StructBench.bench_struct_inout(TestStruct.alloc_inout, 0) }
  }
}
