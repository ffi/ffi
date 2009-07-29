require 'rubygems'
require 'rbconfig'
require 'spec'

if ENV["MRI_FFI"]
  $:.unshift File.join(File.dirname(__FILE__), "..", "..", "lib"),
    File.join(File.dirname(__FILE__), "..", "..", "build", "#{Config::CONFIG['host_cpu''arch']}", "ffi_c", RUBY_VERSION)
end
require "ffi"

module TestLibrary
  PATH = "build/libtest.#{FFI::Platform::LIBSUFFIX}"
end
module LibTest
  extend FFI::Library
  ffi_lib TestLibrary::PATH
end
