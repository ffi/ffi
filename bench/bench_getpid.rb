require File.expand_path(File.join(File.dirname(__FILE__), "bench_helper"))

iter = ITER

module Posix
  extend FFI::Library
  if FFI::Platform.windows?
    attach_function :getpid, :_getpid, [], :uint
  else
    attach_function :getpid, [], :uint
  end
end


puts "pid=#{Process.pid} Foo.getpid=#{Posix.getpid}"
puts "Benchmark FFI getpid performance, #{iter}x calls"
10.times {
  puts Benchmark.measure {
    iter.times { Posix.getpid }
  }
}
puts "Benchmark Process.pid performance, #{iter}x calls"
10.times {
  puts Benchmark.measure {
    iter.times { Process.pid }
  }
}
