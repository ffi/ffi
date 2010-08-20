require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

require 'benchmark'
require 'ffi'
iter = 1000_000

class TestStruct < FFI::Struct
  layout :i, :int, :p, :pointer
end

s = TestStruct.new(FFI::MemoryPointer.new(TestStruct))
puts "Benchmark FFI Struct.get(:int) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { s[:i] }
  }
}

puts "Benchmark FFI Struct.get(:int) using string name performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { s[:i] }
  }
}
puts "Benchmark FFI Struct.put(:int) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { s[:i] = 0x12345678 }
  }
}
puts "Benchmark FFI Struct.get(:pointer) performance, #{iter}x"
10.times {
  puts Benchmark.measure {
    iter.times { s[:p] }
  }
}
puts "Benchmark FFI Struct.put(:pointer) performance, #{iter}x"
10.times {
  p = FFI::MemoryPointer.new :int
  puts Benchmark.measure {
    iter.times { s[:p] = p }
  }
}
puts "Benchmark FFI Struct.get(:string) performance, #{iter}x"
class StringStruct < FFI::Struct
  layout :s, :string
end
10.times {
  mp = FFI::MemoryPointer.new 1024
  mp.put_string(0, "Hello, World")
  s = StringStruct.new
  s.pointer.put_pointer(0, mp)
  puts Benchmark.measure {
    iter.times { s[:s] }
  }
}
