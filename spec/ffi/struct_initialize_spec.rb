require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

describe FFI::Struct, ' with an initialize function' do
  it "should call the initialize function" do
    class StructWithInitialize < FFI::Struct
      layout :string, :string
      attr_accessor :magic
      def initialize
        super
        self.magic = 42
      end
    end
    StructWithInitialize.new.magic.should == 42
  end
end

describe FFI::ManagedStruct, ' with an initialize function' do
  it "should call the initialize function" do
    class ManagedStructWithInitialize < FFI::ManagedStruct
      layout :string, :string
      attr_accessor :magic
      def initialize
        super
        self.magic = 42
      end
    end
    StructWithInitialize.new.magic.should == 42
  end
end
