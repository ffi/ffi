require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))
str = "test"
module LibC
  extend FFI::Library
  attach_function :strlen, [ :string ], :int
end

if LibC.strlen("test") != 4
  raise ArgumentError, "FFI.strlen returned incorrect value"
end

puts "Benchmark FFI api strlen(3) performance, #{ITER}x"
10.times {
  puts Benchmark.measure {
    ITER.times { LibC.strlen(str) }
  }
}

