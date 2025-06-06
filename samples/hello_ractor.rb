require 'ffi'

class Ractor
  # compat with Ruby-3.4 and older
  alias value take unless method_defined? :value
end

module Foo
  extend FFI::Library
  ffi_lib FFI::Library::LIBC
  attach_function("cputs", "puts", [ :string ], :int)
  freeze
end
Ractor.new do
  Foo.cputs("Hello, World via libc puts using FFI in a Ractor")
end.value
