require "rubygems"
require "inline"
require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

module CTest
  extend FFI::Library
  
  attach_function :strcat, [:pointer, :pointer], :pointer
end

describe MemoryPointer do
  it "makes a pointer from a string" do
    m = MemoryPointer.from_string("FFI")
    m.size.should == 3
  end
  
  it "makes a pointer for a certain number of bytes" do
    m = MemoryPointer.new(8)
    m.write_array_of_int([1,2])
    m.read_array_of_int(2).should == [1,2]
  end
  
  it "makes a pointer for a certain type" do
    m = MemoryPointer.new(:int)
    m.write_int(10)
    m.read_int.should == 10
  end
  
  it "makes a memory pointer for a number of a certain type" do
    m = MemoryPointer.new(:int, 2)
    m.write_array_of_int([1,2])
    m.read_array_of_int(2).should == [1,2]
  end
  
  it "makes a pointer for an object responding to #size" do
    m = MemoryPointer.new(Struct.new(:size).new(8))
    m.write_array_of_int([1,2])
    m.read_array_of_int(2).should == [1,2]
  end

  it "makes a pointer for a number of an object responding to #size" do
    m = MemoryPointer.new(Struct.new(:size).new(4), 2)
    m.write_array_of_int([1,2])
    m.read_array_of_int(2).should == [1,2]
  end  
  
end