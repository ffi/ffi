require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

require 'benchmark'
require 'ffi'
iter = 100_000

module BufferBench
  extend FFI::Library
  extend FFI::Library
  ffi_lib LIBTEST_PATH
  attach_function :bench_s32_v, [ :int ], :void
  attach_function :bench_buffer_in, :ptr_ret_int32_t, [ :buffer_in, :int ], :void
  attach_function :bench_buffer_out, :ptr_ret_int32_t, [ :buffer_out, :int ], :void
  attach_function :bench_buffer_inout, :ptr_ret_int32_t, [ :buffer_inout, :int ], :void
end
puts "Benchmark FFI call(MemoryPointer.new) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { BufferBench.bench_buffer_inout(FFI::MemoryPointer.new(:int, 1, false), 0) }
  }
}
puts "Benchmark FFI call(0.chr * 4) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { BufferBench.bench_buffer_inout(0.chr * 4, 0) }
  }
}
puts "Benchmark FFI call(Buffer.alloc_inout) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { BufferBench.bench_buffer_inout(FFI::Buffer.alloc_inout(:int, 1, false), 0) }
  }
}
puts "Benchmark FFI call(Buffer.alloc_in) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { BufferBench.bench_buffer_in(FFI::Buffer.alloc_in(:int, 1, false), 0) }
  }
}
puts "Benchmark FFI call(Buffer.alloc_out) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { BufferBench.bench_buffer_out(FFI::Buffer.alloc_out(:int, 1, false), 0) }
  }
}
