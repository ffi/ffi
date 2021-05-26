
require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

describe "abstract memory" do
  before(:all) do
    FFI.typedef :uint64, :TYPEXXX
    FFI.typedef :uint16, :TYPEXXY
  end

  it 'type size' do
    a = FFI::Buffer.new :TYPEXXX
    b = FFI::Buffer.new :TYPEXXY

    expect(a.size).to eq(8)
    expect(a.size).to eq(FFI::Buffer.new(:uint64).size)

    expect(b.size).to eq(2)
    expect(b.size).to eq(FFI::Buffer.new(:uint16).size)
  end

  it 'method missing' do
    v1, v2, v3, v4 = 0xFE01020304050607, 0xFE01020304050608, 0xFE01020304050609, 0xFE0102030405060A

    aa = FFI::Buffer.new(:TYPEXXX, 4)

    aa.write_TYPEXXX(v1)
    expect(aa.read_TYPEXXX).to eq(v1)
    expect(aa.read_uint64).to eq(v1)

    aa.put_TYPEXXX(8, v2)
    expect(aa.get_TYPEXXX(8)).to eq(v2)

    aa.write_array_of_TYPEXXX([v1, v2, v4, v3])
    expect(aa.read_array_of_TYPEXXX(4)).to eq([v1, v2, v4, v3])

    aa.put_array_of_TYPEXXX 16, [v3, v4]
    expect(aa.get_array_of_TYPEXXX(0, 4)).to eq([v1, v2, v3, v4])
    expect(aa.get_array_of_uint64(0, 4)).to eq([v1, v2, v3, v4])
  end
end
