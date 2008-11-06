require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))
describe "String tests" do
  include FFI
  module LibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH
  end
  it "MemoryPointer#get_string returns a tainted string" do
    mp = MemoryPointer.new 1024
    mp.put_string(0, "test\0")
    str = mp.get_string(0)
    str.tainted?.should == true
  end
end