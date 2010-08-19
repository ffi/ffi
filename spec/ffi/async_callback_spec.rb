require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

describe "async callback" do
  module LibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH
    AsyncIntCallback = callback [ :int ], :void

#    @blocking = true
    attach_function :testAsyncCallback, [ AsyncIntCallback, :int ], :void, :blocking => true
  end

  unless RUBY_VERSION =~ /1.8/
    it ":int (0x7fffffff) argument" do
      v = 0xdeadbeef
      called = false
      cb = Proc.new {|i| v = i; called = true }
      LibTest.testAsyncCallback(cb, 0x7fffffff) 
      called.should be_true
      v.should == 0x7fffffff
    end
  end
end
