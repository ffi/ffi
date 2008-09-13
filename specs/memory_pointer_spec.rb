require "rubygems"
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

  it "allows access to an element of the pointer (as an array)" do
    m = MemoryPointer.new(8)
    m.write_array_of_int([1,2])
    m[0].should == 1
    m[1].should == 2
  end
  
  it "allows writing as an int" do
    m = MemoryPointer.new(:int)
    m.write_int(1)
    m.read_int.should == 1
  end
  
  it "allows writing as a long" do
    m = MemoryPointer.new(:long)
    m.write_long(10)
    m.read_long.should == 10
  end
  
  it "raises an error if you try putting a long into a pointer of size 1" do
    m = MemoryPointer.new(1)
    lambda { m.write_long(10) }.should raise_error
  end
  
  it "raises an error if you try putting an int into a pointer of size 1" do
    m = MemoryPointer.new(1)
    lambda { m.write_int(10) }.should raise_error
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
