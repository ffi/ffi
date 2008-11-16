require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))
describe "Function with primitive integer arguments" do
  module LibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH
    attach_function :ret_int8_t, [ :char ], :char
    attach_function :ret_u_int8_t, [ :uchar ], :uchar
    attach_function :ret_int16_t, [ :short ], :short
    attach_function :ret_u_int16_t, [ :ushort ], :ushort
    attach_function :ret_int32_t, [ :int ], :int
    attach_function :ret_u_int32_t, [ :uint ], :uint
  end
  [ 0, 127, -128, -1 ].each do |i|
    it ":char call(:char (#{i}))" do
      LibTest.ret_int8_t(i).should == i
    end
  end
  [ 0, 0x7f, 0x80, 0xff ].each do |i|
    it ":uchar call(:uchar (#{i}))" do
      LibTest.ret_u_int8_t(i).should == i
    end
  end
  [ 0, 0x7fff, -0x8000, -1 ].each do |i|
    it ":short call(:short (#{i}))" do
      LibTest.ret_int16_t(i).should == i
    end
  end
  [ 0, 0x7fff, 0x8000, 0xffff ].each do |i|
    it ":ushort call(:ushort (#{i}))" do
      LibTest.ret_u_int16_t(i).should == i
    end
  end
  [ 0, 0x7fffffff, -0x80000000, -1 ].each do |i|
    it ":int call(:int (#{i}))" do
      LibTest.ret_int32_t(i).should == i
    end
  end
  [ 0, 0x7fffffff, 0x80000000, 0xffffffff ].each do |i|
    it ":uint call(:uint (#{i}))" do
      LibTest.ret_u_int32_t(i).should == i
    end
  end
end
describe "Integer parameter range checking" do
  [ 128, -129 ].each do |i|
    it ":char call(:char (#{i}))" do
      lambda { LibTest.ret_int8_t(i).should == i }.should raise_error
    end
  end
  [ -1, 256 ].each do |i|
    it ":uchar call(:uchar (#{i}))" do
      lambda { LibTest.ret_u_int8_t(i).should == i }.should raise_error
    end
  end
  [ 0x8000, -0x8001 ].each do |i|
    it ":short call(:short (#{i}))" do
      lambda { LibTest.ret_int16_t(i).should == i }.should raise_error
    end
  end
  [ -1, 0x10000 ].each do |i|
    it ":ushort call(:ushort (#{i}))" do
      lambda { LibTest.ret_u_int16_t(i).should == i }.should raise_error
    end
  end
  [ 0x80000000, -0x80000001 ].each do |i|
    it ":int call(:int (#{i}))" do
      lambda { LibTest.ret_int32_t(i).should == i }.should raise_error
    end
  end
  [ -1, 0x100000000 ].each do |i|
    it ":ushort call(:ushort (#{i}))" do
      lambda { LibTest.ret_u_int32_t(i).should == i }.should raise_error
    end
  end
end
describe "Three different size Integer arguments" do
  TYPE_MAP = {
    's8' => :char, 'u8' => :uchar, 's16' => :short, 'u16' => :ushort,
    's32' => :int, 'u32' => :uint, 's64' => :long_long, 'u64' => :ulong_long,
    'sL' => :long, 'uL' => :ulong
  }
  TYPES = TYPE_MAP.keys
  module LibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH
    
    
    [ 's32', 'u32', 's64', 'u64' ].each do |rt|
      TYPES.each do |t1|
        TYPES.each do |t2|
          TYPES.each do |t3|
            begin
              attach_function "pack_#{t1}#{t2}#{t3}_#{rt}",
                [ TYPE_MAP[t1], TYPE_MAP[t2], TYPE_MAP[t3], :buffer_out ], :void
            rescue FFI::NotFoundError
            end
          end
        end
      end
    end
  end

  PACK_VALUES = {
    's8' => [ 0x12  ],
    'u8' => [ 0x34  ],
    's16' => [ 0x5678 ],
    'u16' => [ 0x9abc ],
    's32' => [ 0x7654321f ],
    'u32' => [ 0xfee1babe ],
    'sL' => [ 0x1f2e3d4c ],
    'uL' => [ 0xf7e8d9ca ],
    's64' => [ 0x1eafdeadbeefa1b2 ],
  }
  PACK_VALUES.keys.each do |t1|
    PACK_VALUES.keys.each do |t2|
      PACK_VALUES.keys.each do |t3|
        PACK_VALUES[t1].each do |v1|
          PACK_VALUES[t2].each do |v2|
            PACK_VALUES[t3].each do |v3|
              it "call(#{TYPE_MAP[t1]} (#{v1}), #{TYPE_MAP[t2]} (#{v2}), #{TYPE_MAP[t3]} (#{v3}))" do
                p = FFI::Buffer.new :long_long, 3
                LibTest.send("pack_#{t1}#{t2}#{t3}_s64", v1, v2, v3, p)
                p.get_int64(0).should == v1
                p.get_int64(8).should == v2
                p.get_int64(16).should == v3
              end
            end
          end
        end
      end
    end
  end
end
