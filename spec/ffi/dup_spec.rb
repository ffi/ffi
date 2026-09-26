#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#

require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

describe "Pointer#dup" do
  it "clone should be independent" do
    p1 = FFI::MemoryPointer.new(:char, 1024)
    p1.put_string(0, "test123");
    p2 = p1.dup
    p1.put_string(0, "deadbeef")

    expect(p2.get_string(0)).to eq("test123")
  end

  it "sliced pointer can be cloned" do
    p1 = FFI::MemoryPointer.new(:char, 1024)
    p1.put_string(0, "test123");
    p2 = p1[1].dup

    # first char will be excised
    expect(p2.get_string(0)).to eq("est123")
    expect(p1.get_string(0)).to eq("test123")
  end

  it "sliced pointer when cloned is independent" do
    p1 = FFI::MemoryPointer.new(:char, 1024)
    p1.put_string(0, "test123");
    p2 = p1[1].dup

    p1.put_string(0, "deadbeef")
    # first char will be excised
    expect(p2.get_string(0)).to eq("est123")
  end
end


describe "Struct#dup" do
  it "clone should be independent" do
    s = Class.new(FFI::Struct) do
      layout :i, :int
    end
    s1 = s.new
    s1[:i] = 0x12345
    s2 = s1.dup
    s1[:i] = 0x98765
    expect(s2[:i]).to eq(0x12345)
    expect(s1[:i]).to eq(0x98765)
  end

  # The storage behind the reference fields is allocated lazily, so it is still
  # absent until one of them is assigned. Copying a struct in that state used to
  # read from a null pointer and crash the VM.
  def struct_with_unassigned_reference
    klass = Class.new(FFI::Struct) do
      layout :ptr, :pointer, :i, :int
    end
    s = klass.new
    s[:i] = 0x12345
    s
  end

  it "can dup a struct whose reference fields were never assigned" do
    s2 = struct_with_unassigned_reference.dup
    expect(s2[:i]).to eq(0x12345)
    expect(s2[:ptr]).to be_null
  end

  it "can clone a struct whose reference fields were never assigned" do
    s2 = struct_with_unassigned_reference.clone
    expect(s2[:i]).to eq(0x12345)
    expect(s2[:ptr]).to be_null
  end

  it "can set the byte order of a struct whose reference fields were never assigned" do
    # #order dups the struct internally, so it reaches the same copy path.
    # Reading the pointer field back is deliberately left out: a native address
    # in non-native byte order is not meaningful, and JRuby rejects it outright.
    skip "not yet supported on TruffleRuby" if RUBY_ENGINE == "truffleruby"
    s1 = struct_with_unassigned_reference
    expect { s1.order(:big) }.not_to raise_error
  end

  it "copies an assigned reference field into an independent struct" do
    klass = Class.new(FFI::Struct) do
      layout :ptr, :pointer, :i, :int
    end
    p1 = FFI::MemoryPointer.new(:int, 1)
    p2 = FFI::MemoryPointer.new(:int, 1)

    s1 = klass.new
    s1[:ptr] = p1
    s2 = s1.dup
    expect(s2[:ptr].address).to eq(p1.address)

    s1[:ptr] = p2
    expect(s2[:ptr].address).to eq(p1.address)
    expect(s1[:ptr].address).to eq(p2.address)
  end
end
