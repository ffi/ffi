require 'benchmark'

lib = File.expand_path('../../lib', __FILE__)

cext = "#{lib}/ffi_c.so"
unless File.exist?(cext)
  abort "#{cext} is not compiled. Compile it with 'rake compile' first."
end

$LOAD_PATH.unshift(lib)
require 'ffi'

ITER = ENV['ITER'] ? ENV['ITER'].to_i : 100_000

LIBTEST_PATH = File.expand_path("../../spec/ffi/fixtures/libtest.#{FFI::Platform::LIBSUFFIX}", __FILE__)
