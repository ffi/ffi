#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#

require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

module UnionSpec
module LibTest
  extend FFI::Library
  ffi_lib TestLibrary::PATH

  Types = {
    's8' => [:char, :c, 1],
    's16' => [:short, :s, 0xff0],
    's32' => [:int, :i, 0xff00],
    's64' => [:long_long, :j, 0xffff00],
    'long' => [:long, :l, 0xffff],
    'f32' => [:float, :f, 1.0001],
    'f64' => [:double, :d, 1.000000001]
  }

  class TestUnion < FFI::Union
    layout( :a, [:char, 10],
            :i, :int,
            :f, :float,
            :d, :double,
            :s, :short,
            :l, :long,
            :j, :long_long,
            :c, :char )
  end

  Types.keys.each do |k|
    attach_function "union_align_#{k}", [ :pointer ], Types[k][0]
    attach_function "union_make_union_with_#{k}", [ Types[k][0] ], :pointer
  end
  attach_function :union_size, [], :uint
end

describe 'Union' do
  before do
    @u = LibTest::TestUnion.new
  end

  it 'should place all the fields at offset 0' do
    expect(LibTest::TestUnion.members.all? { |m| LibTest::TestUnion.offset_of(m) == 0 }).to be true
  end

  LibTest::Types.each do |k, (type, name, val)|
    it "should correctly align/write a #{type} value" do
      @u[name] = val
      if k == 'f32' or k == 'f64'
        expect((@u[name] - LibTest.send("union_align_#{k}", @u.to_ptr)).abs).to be < 0.00001
      else
        expect(@u[name]).to eq(LibTest.send("union_align_#{k}", @u.to_ptr))
      end
    end
  end

  LibTest::Types.each do |k, (type, name, val)|
    it "should read a #{type} value from memory" do
      @u = LibTest::TestUnion.new(LibTest.send("union_make_union_with_#{k}", val))
      if k == 'f32' or k == 'f64'
        expect((@u[name] - val).abs).to be < 0.00001
      else
        expect(@u[name]).to eq(val)
      end
    end
  end

  it 'should return a size equals to the size of the biggest field' do
    expect(LibTest::TestUnion.size).to eq(LibTest.union_size)
  end
end

describe 'Union by_value' do
  module ByValueLibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH

    class PjXyzt < FFI::Struct
      layout :x, :double, :y, :double, :z, :double, :t, :double
    end

    class UnionDouble < FFI::Union
      layout :v, [:double, 4],
             :coord, PjXyzt
    end

    attach_function :union_double_coord, [:double, :double, :double, :double], UnionDouble.by_value
    attach_function :union_double_get_x, [UnionDouble.by_value], :double
    attach_function :union_double_get_y, [UnionDouble.by_value], :double
    attach_function :union_double_get_z, [UnionDouble.by_value], :double
    attach_function :union_double_get_t, [UnionDouble.by_value], :double
    attach_function :union_double_add, [UnionDouble.by_value, UnionDouble.by_value], UnionDouble.by_value
  end

  it 'should return a union of doubles by value' do
    u = ByValueLibTest.union_double_coord(1.5, 2.5, 3.5, 4.5)
    expect(u[:coord][:x]).to eq(1.5)
    expect(u[:coord][:y]).to eq(2.5)
    expect(u[:coord][:z]).to eq(3.5)
    expect(u[:coord][:t]).to eq(4.5)
  end

  it 'should pass a union of doubles by value' do
    u = ByValueLibTest.union_double_coord(10.0, 20.0, 30.0, 40.0)
    expect(ByValueLibTest.union_double_get_x(u)).to eq(10.0)
    expect(ByValueLibTest.union_double_get_y(u)).to eq(20.0)
    expect(ByValueLibTest.union_double_get_z(u)).to eq(30.0)
    expect(ByValueLibTest.union_double_get_t(u)).to eq(40.0)
  end

  it 'should pass and return a union of doubles by value' do
    a = ByValueLibTest.union_double_coord(1.0, 2.0, 3.0, 4.0)
    b = ByValueLibTest.union_double_coord(0.5, 0.5, 0.5, 0.5)
    r = ByValueLibTest.union_double_add(a, b)
    expect(r[:coord][:x]).to eq(1.5)
    expect(r[:coord][:y]).to eq(2.5)
    expect(r[:coord][:z]).to eq(3.5)
    expect(r[:coord][:t]).to eq(4.5)
  end

  it 'should access union doubles via array field' do
    u = ByValueLibTest.union_double_coord(1.5, 2.5, 3.5, 4.5)
    expect(u[:v].to_a).to eq([1.5, 2.5, 3.5, 4.5])
  end
end if RUBY_ENGINE != "truffleruby"
end
