require 'benchmark'

$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)
require 'ffi'

ITER = ENV['ITER'] ? ENV['ITER'].to_i : 100_000

LIBTEST_PATH = File.expand_path("../../spec/ffi/fixtures/libtest.#{FFI::Platform::LIBSUFFIX}", __FILE__)
