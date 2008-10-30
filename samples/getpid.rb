require 'rubygems'
require 'ffi'
module Foo
  extend FFI::Library
  attach_function :getpid, [ ], :int
end
puts "My pid=#{Foo.getpid}"
