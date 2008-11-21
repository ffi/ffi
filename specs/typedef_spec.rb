require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))
describe "Custom type definitions" do
  it "attach_function with custom typedef" do
    module CustomTypedef
      extend FFI::Library
      ffi_lib TestLibrary::PATH
      typedef :uint, :fubar_t
      attach_function :ret_u_int32_t, [ :fubar_t ], :fubar_t
    end
    CustomTypedef.ret_u_int32_t(0x12345678).should == 0x12345678
  end
  it "variadic invoker with custom typedef" do
    module VariadicCustomTypedef
      extend FFI::Library
      ffi_lib TestLibrary::PATH
      typedef :uint, :fubar_t
      attach_function :pack_varargs, [ :buffer_out, :string, :varargs ], :void
    end
    buf = FFI::Buffer.new :uint, 10
    VariadicCustomTypedef.pack_varargs(buf, "i", :fubar_t, 0x12345678)
    buf.get_uint(0).should == 0x12345678
  end
  it "Callback with custom typedef parameter" do
    module CallbackCustomTypedef
      extend FFI::Library
      ffi_lib TestLibrary::PATH
      typedef :uint, :fubar3_t
      callback :cbIrV, [ :fubar3_t ], :void
      attach_function :testCallbackU32rV, :testClosureIrV, [ :cbIrV, :fubar3_t ], :void
    end
    i = 0
    CallbackCustomTypedef.testCallbackU32rV(0xdeadbeef) { |v| i = v }
    i.should == 0xdeadbeef
  end
end