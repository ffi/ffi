require "rubygems"
require 'benchmark'
require 'ffi'
ITER = ENV['ITER'] ? ENV['ITER'].to_i : 100000
LIBTEST_PATH = "#{Dir.getwd}/build/libtest.#{FFI::Platform::LIBSUFFIX}"
class FFI::Invoker
  # Add arity-specific call paths for older ruby-ffi versions
  alias :call0 call unless method_defined?("call0")
  alias :call1 call unless method_defined?("call1")
  alias :call2 call unless method_defined?("call2")
  alias :call3 call unless method_defined?("call3")
end
