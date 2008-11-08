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
  it "Struct#layout works with :name, :type pairs" do
    class PairLayout < FFI::Struct
      layout :a, :int, :b, :long_long
    end
    PairLayout.size.should == 12
    mp = MemoryPointer.new(12)
    s = PairLayout.new mp
    s[:a] = 0x12345678
    mp.get_int(0).should == 0x12345678
    s[:b] = 0xfee1deadbeef
    mp.get_int64(4).should == 0xfee1deadbeef
  end
  it "Struct#layout works with :name, :type, offset tuples" do
    class PairLayout < FFI::Struct
      layout :a, :int, 0, :b, :long_long, 4
    end
    PairLayout.size.should == 12
    mp = MemoryPointer.new(12)
    s = PairLayout.new mp
    s[:a] = 0x12345678
    mp.get_int(0).should == 0x12345678
    s[:b] = 0xfee1deadbeef
    mp.get_int64(4).should == 0xfee1deadbeef
  end
  it "Struct#layout works with mixed :name,:type and :name,:type,offset" do
    class MixedLayout < FFI::Struct
      layout :a, :int, :b, :long_long, 4
    end
    MixedLayout.size.should == 12
    mp = MemoryPointer.new(12)
    s = MixedLayout.new mp
    s[:a] = 0x12345678
    mp.get_int(0).should == 0x12345678
    s[:b] = 0xfee1deadbeef
    mp.get_int64(4).should == 0xfee1deadbeef
  end
  rb_maj, rb_min = RUBY_VERSION.split('.')
  if rb_maj.to_i >= 1 && rb_min.to_i >= 9 || RUBY_PLATFORM =~ /java/
    it "Struct#layout withs with a hash of :name => type" do
      class HashLayout < FFI::Struct
        layout :a => :int, :b => :long_long
      end
      HashLayout.size.should == 12
      mp = MemoryPointer.new(12)
      s = HashLayout.new mp
      s[:a] = 0x12345678
      mp.get_int(0).should == 0x12345678
      s[:b] = 0xfee1deadbeef
      mp.get_int64(4).should == 0xfee1deadbeef
      end
  end
end
