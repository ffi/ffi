$:.unshift File.join(File.dirname(__FILE__), "..", "lib") if ENV["MRI_FFI"]
require "ffi"
