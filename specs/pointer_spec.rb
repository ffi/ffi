require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))
require 'delegate'
describe "Pointer" do
  include FFI
  module LibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH
    attach_function :ptr_ret_int32_t, [ :pointer, :int ], :int
    attach_function :ptr_from_address, [ FFI::Platform::ADDRESS_SIZE == 32 ? :uint : :ulong_long ], :pointer
  end
  class ToPtrTest
    def initialize(ptr)
      @ptr = ptr
    end
    def to_ptr
      @ptr
    end
  end
  it "Any object implementing #to_ptr can be passed as a :pointer parameter" do
    memory = MemoryPointer.new :long_long
    magic = 0x12345678
    memory.put_int32(0, magic)
    tp = ToPtrTest.new(memory)
    LibTest.ptr_ret_int32_t(tp, 0).should == magic
  end
  class PointerDelegate < DelegateClass(FFI::Pointer)
    def initialize(ptr)
      super
      @ptr = ptr
    end
    def to_ptr
      @ptr
    end
  end
  it "A DelegateClass(Pointer) can be passed as a :pointer parameter" do
    memory = MemoryPointer.new :long_long
    magic = 0x12345678
    memory.put_int32(0, magic)
    ptr = PointerDelegate.new(memory)
    LibTest.ptr_ret_int32_t(ptr, 0).should == magic
  end
end

describe "autopointers get cleaned up properly" do
  loop_count = 30
  wiggle_room = 2 # GC rarely cleans up all objects. we can get most of them, and that's enough to determine if the basic functionality is working.
  magic = 0x12345678

  class AutoPointerTestHelper
    @@count = 0
    def self.release
      @@count += 1 if @@count > 0
    end
    def self.reset
      @@count = 0
    end
    def self.gc_everything(count)
      loop = 5
      while @@count < count && loop > 0
        loop -= 1
        GC.start
        sleep 0.05 unless @@count == count
      end
      @@count = 0
    end
    def self.finalizer
      self.method(:release).to_proc
    end
  end

  it "when using default release method" do
    FFI::AutoPointer.should_receive(:release).at_least(loop_count-wiggle_room).times
    AutoPointerTestHelper.reset
    loop_count.times do
      # note that if we called
      # AutoPointerTestHelper.method(:release).to_proc inline, we'd
      # have a reference to the pointer and it would never get GC'd.
      ap = FFI::AutoPointer.new(LibTest.ptr_from_address(magic))
    end
    AutoPointerTestHelper.gc_everything loop_count
  end

  it "when passed a proc" do
    #  NOTE: passing a proc is touchy, because it's so easy to create a memory leak.
    #
    #  specifically, if we made an inline call to
    #
    #      AutoPointerTestHelper.method(:release).to_proc
    #
    #  we'd have a reference to the pointer and it would
    #  never get GC'd.
    AutoPointerTestHelper.should_receive(:release).at_least(loop_count-wiggle_room).times
    AutoPointerTestHelper.reset
    loop_count.times do
      ap = FFI::AutoPointer.new(LibTest.ptr_from_address(magic),
                                AutoPointerTestHelper.finalizer)
    end
    AutoPointerTestHelper.gc_everything loop_count
  end

  it "when passed a method" do
    AutoPointerTestHelper.should_receive(:release).at_least(loop_count-wiggle_room).times
    AutoPointerTestHelper.reset
    loop_count.times do
      ap = FFI::AutoPointer.new(LibTest.ptr_from_address(magic),
                                AutoPointerTestHelper.method(:release))
    end
    AutoPointerTestHelper.gc_everything loop_count
  end
end
describe "AutoPointer argument checking" do
  it "Should raise error when passed a MemoryPointer" do
    lambda { FFI::AutoPointer.new(FFI::MemoryPointer.new(:int))}.should raise_error(ArgumentError)
  end
  it "Should raise error when passed a AutoPointer" do
    lambda { FFI::AutoPointer.new(FFI::AutoPointer.new(LibTest.ptr_from_address(0))) }.should raise_error(ArgumentError)
  end
  it "Should raise error when passed a Buffer" do
    lambda { FFI::AutoPointer.new(FFI::Buffer.new(:int))}.should raise_error(ArgumentError)
  end

end
