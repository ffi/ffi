require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))
describe "Struct tests" do
  include FFI
  StructTypes = {
    's8' => :char,
    's16' => :short,
    's32' => :int,
    's64' => :long_long,
    'long' => :long,
    'f32' => :float,
    'f64' => :double
  }
  module LibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH
    attach_function :ptr_ret_pointer, [ :pointer, :int], :string
    attach_function :string_equals, [ :string, :string ], :int
    [ 's8', 's16', 's32', 's64', 'f32', 'f64', 'long' ].each do |t|
      attach_function "struct_align_#{t}", [ :pointer ], StructTypes[t]
    end
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
  it "Can use Struct subclass as parameter type" do
    module StructParam
      extend FFI::Library
      ffi_lib TestLibrary::PATH
      class TestStruct < FFI::Struct
        layout :c, :char
      end
      attach_function :struct_field_s8, [ TestStruct ], :char
    end
  end
  it "Can use Struct subclass as IN parameter type" do
    module StructParam
      extend FFI::Library
      ffi_lib TestLibrary::PATH
      class TestStruct < FFI::Struct
        layout :c, :char
      end
      attach_function :struct_field_s8, [ TestStruct.in ], :char
    end
  end
  it "Can use Struct subclass as OUT parameter type" do
    module StructParam
      extend FFI::Library
      ffi_lib TestLibrary::PATH
      class TestStruct < FFI::Struct
        layout :c, :char
      end
      attach_function :struct_field_s8, [ TestStruct.out ], :char
    end
  end
  it ":char member aligned correctly" do
    class AlignChar < FFI::Struct
      layout :c, :char, :v, :char
    end
    s = AlignChar.new
    s[:v] = 0x12
    LibTest.struct_align_s8(s.pointer).should == 0x12
  end
  it ":short member aligned correctly" do
    class AlignShort < FFI::Struct
      layout :c, :char, :v, :short
    end
    s = AlignShort.alloc_in
    s[:v] = 0x1234
    LibTest.struct_align_s16(s.pointer).should == 0x1234
  end
  it ":int member aligned correctly" do
    class AlignInt < FFI::Struct
      layout :c, :char, :v, :int
    end
    s = AlignInt.alloc_in
    s[:v] = 0x12345678
    LibTest.struct_align_s32(s.pointer).should == 0x12345678
  end
  it ":long_long member aligned correctly" do
    class AlignLongLong < FFI::Struct
      layout :c, :char, :v, :long_long
    end
    s = AlignLongLong.alloc_in
    s[:v] = 0x123456789abcdef0
    LibTest.struct_align_s64(s.pointer).should == 0x123456789abcdef0
  end
  it ":long member aligned correctly" do
    class AlignLong < FFI::Struct
      layout :c, :char, :v, :long
    end
    s = AlignLong.alloc_in
    s[:v] = 0x12345678
    LibTest.struct_align_long(s.pointer).should == 0x12345678
  end
  it ":float member aligned correctly" do
    class AlignFloat < FFI::Struct
      layout :c, :char, :v, :float
    end
    s = AlignFloat.alloc_in
    s[:v] = 1.23456
    (LibTest.struct_align_f32(s.pointer) - 1.23456).abs.should < 0.00001
  end
  it ":double member aligned correctly" do
    class AlignDouble < FFI::Struct
      layout :c, :char, :v, :double
    end
    s = AlignDouble.alloc_in
    s[:v] = 1.23456789
    (LibTest.struct_align_f64(s.pointer) - 1.23456789).abs.should < 0.00000001
  end
end
