require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))
describe "Struct tests" do
  include FFI
  module LibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH
    attach_function :ptr_ret_pointer, [ :pointer, :int], :string
    attach_function :string_equals, [ :string, :string ], :int
  end
  class PointerMember < FFI::Struct
    layout :pointer, :pointer, 0
  end
  class StringMember < FFI::Struct
    layout :string, :string, 0
  end
  it "Struct#[:pointer]" do
    magic = 0x12345678
    mp = MemoryPointer.new :long
    mp.put_long(0, magic)
    smp = MemoryPointer.new :pointer
    smp.put_pointer(0, mp)
    s = PointerMember.new smp
    s[:pointer].should == mp
  end
  it "Struct#[:pointer].nil? for NULL value" do
    magic = 0x12345678
    mp = MemoryPointer.new :long
    mp.put_long(0, magic)
    smp = MemoryPointer.new :pointer
    smp.put_pointer(0, nil)
    s = PointerMember.new smp
    s[:pointer].null?.should == true
  end
  it "Struct#[:pointer]=" do
    magic = 0x12345678
    mp = MemoryPointer.new :long
    mp.put_long(0, magic)
    smp = MemoryPointer.new :pointer
    s = PointerMember.new smp
    s[:pointer] = mp
    smp.get_pointer(0).should == mp
  end
  it "Struct#[:pointer]=nil" do
    smp = MemoryPointer.new :pointer
    s = PointerMember.new smp
    s[:pointer] = nil
    smp.get_pointer(0).null?.should == true
  end
  it "Struct#[:string]" do
    magic = "test"
    mp = MemoryPointer.new 1024
    mp.put_string(0, magic)
    smp = MemoryPointer.new :pointer
    smp.put_pointer(0, mp)
    s = StringMember.new smp
    s[:string].should == magic
  end
  it "Struct#[:string].nil? for NULL value" do
    smp = MemoryPointer.new :pointer
    smp.put_pointer(0, nil)
    s = StringMember.new smp
    s[:string].nil?.should == true
  end
end
