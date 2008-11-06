require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))
describe "String tests" do
  include FFI
  module LibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH
    attach_function :ptr_ret_pointer, [ :pointer, :int], :string
    attach_function :string_equals, [ :string, :string ], :int
  end
  it "MemoryPointer#get_string returns a tainted string" do
    mp = MemoryPointer.new 1024
    mp.put_string(0, "test\0")
    str = mp.get_string(0)
    str.tainted?.should == true
  end
  it "String returned by a method is tainted" do
    mp = MemoryPointer.new :pointer
    sp = MemoryPointer.new 1024
    sp.put_string(0, "test")
    mp.put_pointer(0, sp)
    str = LibTest.ptr_ret_pointer(mp, 0)
    str.should == "test"
    str.tainted?.should == true
  end
  it "Tainted String parameter should throw a SecurityError" do
    $SAFE = 1
    str = "test"
    str.taint
    begin
      LibTest.string_equals(str, str).should == false
    rescue SecurityError => e
    end
  end
end
