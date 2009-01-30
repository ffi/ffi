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
    attach_function :ptr_from_address, [ :ulong ], :pointer
    attach_function :string_equals, [ :string, :string ], :int
    [ 's8', 's16', 's32', 's64', 'f32', 'f64', 'long' ].each do |t|
      attach_function "struct_align_#{t}", [ :pointer ], StructTypes[t]
    end
  end
  class PointerMember < FFI::Struct
    layout :pointer, :pointer
  end
  class StringMember < FFI::Struct
    layout :string, :string
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
    ll_off = (FFI::Platform::ADDRESS_SIZE == 32 ? 4 : 8)
    PairLayout.size.should == (ll_off + 8)
    mp = MemoryPointer.new(PairLayout.size)
    s = PairLayout.new mp
    s[:a] = 0x12345678
    mp.get_int(0).should == 0x12345678
    s[:b] = 0xfee1deadbeef
    mp.get_int64(ll_off).should == 0xfee1deadbeef
  end
  it "Struct#layout works with :name, :type, offset tuples" do
    class PairLayout < FFI::Struct
      layout :a, :int, 0, :b, :long_long, 4
    end
    PairLayout.size.should == 12
    mp = MemoryPointer.new(PairLayout.size)
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
    PairLayout.size.should == 12
    mp = MemoryPointer.new(PairLayout.size)
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
      ll_off = (FFI::Platform::ADDRESS_SIZE == 32 ? 4 : 8)
      HashLayout.size.should == (ll_off + 8)
      mp = MemoryPointer.new(HashLayout.size)
      s = HashLayout.new mp
      s[:a] = 0x12345678
      mp.get_int(0).should == 0x12345678
      s[:b] = 0xfee1deadbeef
      mp.get_int64(ll_off).should == 0xfee1deadbeef
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
  it "Struct can be passed directly as a :pointer parameter" do
    class TestStruct < FFI::Struct
      layout :c, :char, :v, :char
    end
    s = TestStruct.new
    s[:v] = 0x12
    LibTest.struct_align_s8(s).should == 0x12
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
  it ":ulong, :pointer struct" do
    class ULPStruct < FFI::Struct
      layout :ul, :ulong, :p, :pointer
    end
    s = ULPStruct.alloc_in
    s[:ul] = 0xdeadbeef
    s[:p] = LibTest.ptr_from_address(0x12345678)
    s.pointer.get_ulong(0).should == 0xdeadbeef
  end
  def test_num_field(type, v)
    klass = Class.new(FFI::Struct)
    klass.layout :v, type, :dummy, :long
    
    s = klass.new
    s[:v] = v
    s.pointer.send("get_#{type.to_s}", 0).should == v
    s.pointer.send("put_#{type.to_s}", 0, 0)
    s[:v].should == 0
  end
  def self.int_field_test(type, values)
    values.each do |v|
      it "#{type} field r/w (#{v.to_s(16)})" do
        test_num_field(type, v)
      end
    end
  end
  int_field_test(:char, [ 0, 127, -128, -1 ])
  int_field_test(:uchar, [ 0, 0x7f, 0x80, 0xff ])
  int_field_test(:short, [ 0, 0x7fff, -0x8000, -1 ])
  int_field_test(:ushort, [ 0, 0x7fff, 0x8000, 0xffff ])
  int_field_test(:int, [ 0, 0x7fffffff, -0x80000000, -1 ])
  int_field_test(:uint, [ 0, 0x7fffffff, 0x80000000, 0xffffffff ])
  int_field_test(:long_long, [ 0, 0x7fffffffffffffff, -0x8000000000000000, -1 ])
  int_field_test(:ulong_long, [ 0, 0x7fffffffffffffff, 0x8000000000000000, 0xffffffffffffffff ])
  if FFI::Platform::LONG_SIZE == 32
    int_field_test(:long, [ 0, 0x7fffffff, -0x80000000, -1 ])
    int_field_test(:ulong, [ 0, 0x7fffffff, 0x80000000, 0xffffffff ])
  else
    int_field_test(:long, [ 0, 0x7fffffffffffffff, -0x8000000000000000, -1 ])
    int_field_test(:ulong, [ 0, 0x7fffffffffffffff, 0x8000000000000000, 0xffffffffffffffff ])
  end
  it ":float field r/w" do
    klass = Class.new(FFI::Struct)
    klass.layout :v, :float, :dummy, :long

    s = klass.new
    value = 1.23456
    s[:v] = value
    (s.pointer.get_float(0) - value).abs.should < 0.0001
  end
  it ":double field r/w" do
    klass = Class.new(FFI::Struct)
    klass.layout :v, :double, :dummy, :long

    s = klass.new
    value = 1.23456
    s[:v] = value
    (s.pointer.get_double(0) - value).abs.should < 0.0001
  end
  module CallbackMember
    extend FFI::Library
    ffi_lib TestLibrary::PATH
    callback :add, [ :int, :int ], :int
    callback :sub, [ :int, :int ], :int
    class TestStruct < FFI::Struct
      layout :add, :add,
        :sub, :sub
    end
    attach_function :struct_call_add_cb, [TestStruct, :int, :int], :int
    attach_function :struct_call_sub_cb, [TestStruct, :int, :int], :int
  end
  it "Can have CallbackInfo struct field" do
      s = CallbackMember::TestStruct.new
      add_proc = lambda { |a, b| a+b }
      sub_proc = lambda { |a, b| a-b }
      s[:add] = add_proc
      s[:sub] = sub_proc
      CallbackMember.struct_call_add_cb(s.pointer, 40, 2).should == 42
      CallbackMember.struct_call_sub_cb(s.pointer, 44, 2).should == 42
  end
  it "Can return its members as a list" do
    class TestStruct < FFI::Struct
      layout :a, :int, :b, :int, :c, :int
    end
    TestStruct.members.should include(:a, :b, :c)
  end
  it "Can return its instance members and values as lists" do
    class TestStruct < FFI::Struct
      layout :a, :int, :b, :int, :c, :int
    end
    s = TestStruct.new
    s.members.should include(:a, :b, :c)
    s[:a] = 1
    s[:b] = 2
    s[:c] = 3
    s.values.should include(1, 2, 3)
  end
end

describe FFI::Struct, ' with a nested struct field'  do
  module LibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH
    class NestedStruct < FFI::Struct
      layout :i, :int
    end
    class ContainerStruct < FFI::Struct
      layout :first, :char, :ns, NestedStruct
    end
    attach_function :struct_align_nested_struct, [ :pointer ], :int
    attach_function :struct_make_container_struct, [ :int ], :pointer
  end
  before do
    @cs = LibTest::ContainerStruct.new
  end
  it 'should align correctly nested struct field' do
    @cs[:ns][:i] = 123
    LibTest.struct_align_nested_struct(@cs.to_ptr).should == 123
  end
  it 'should correctly calculate Container size (in bytes)' do
    LibTest::ContainerStruct.size.should == 8
  end
  it 'should return a Struct object when the field is accessed' do
    @cs[:ns].is_a?(FFI::Struct).should be_true 
  end
  it 'should read a value from memory' do
    @cs = LibTest::ContainerStruct.new(LibTest.struct_make_container_struct(123))
    @cs[:ns][:i].should == 123
  end
  it 'should write a value to memory' do
    @cs = LibTest::ContainerStruct.new(LibTest.struct_make_container_struct(123))
    @cs[:ns][:i] = 456
    LibTest.struct_align_nested_struct(@cs.to_ptr).should == 456
  end
end

describe FFI::Struct, ' with an array field'  do
  module LibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH
    class StructWithArray < FFI::Struct
      layout :first, :char, :a, [:int, 5]
    end
    attach_function :struct_make_struct_with_array, [:int, :int, :int, :int, :int], :pointer
  end
  before do
    @s = LibTest::StructWithArray.new
  end
  it 'should align correctly array field'
  it 'should correctly calculate StructWithArray size (in bytes)' do
    LibTest::StructWithArray.size.should == 24
  end
  it 'should return a Struct::Array object when the field is accessed' do
    @s[:a].is_a?(FFI::Struct::Array).should be_true
  end
  it 'should cache Struct::Array object for successive calls' do
    @s[:a].object_id.should == @s[:a].object_id
  end
end

describe 'Struct::Array' do
  module ArrayTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH
    class StructWithArray < FFI::Struct
      layout( :c, :char, 
              :a, [:int, 5] )
    end
    attach_function :struct_make_struct_with_array, [:int, :int, :int, :int, :int], :pointer
    attach_function :struct_field_array, [:pointer], :pointer
  end
  before do
    @struct = ArrayTest::StructWithArray.new(ArrayTest.struct_make_struct_with_array(0, 1, 2, 3, 4))
    @array = FFI::Struct::Array.new(ArrayTest.struct_field_array(@struct.to_ptr), FFI::StructLayoutBuilder::Signed32, 5)
  end
  it 'should return a ruby array' do
    @array.to_a.should == [0, 1, 2, 3, 4]
  end
  it 'should cache array object for successive calls' do
    @array.to_a.object_id.should == @array.to_a.object_id
  end
  it 'should return a pointer' do
    @array.to_ptr.is_a?(FFI::Pointer)
  end
  it 'should return its size in byte' do
    @array.size.should == 20
  end
end
