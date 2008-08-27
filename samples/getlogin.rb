require 'ffi'
module Foo
  extend FFI::Library
  attach_function :getlogin, [ ], :string
end
puts "getlogin=#{Foo.getlogin}"
