require "rubygems"
#require 'ffi/times' if RUBY_PLATFORM =~ /java/
require 'benchmark'
require 'ffi'
ITER = ENV['ITER'] ? ENV['ITER'].to_i : 100000
LIBTEST_PATH = File.expand_path("../../spec/ffi/fixtures/libtest.#{FFI::Platform::LIBSUFFIX}", __FILE__)
