require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

describe "async callback" do
  module LibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH
    AsyncIntCallback = callback [ :int ], :void

    @blocking = true
    attach_function :testAsyncCallback, [ AsyncIntCallback, :int ], :void
  end

  unless RUBY_VERSION =~ /1.8/
    it ":int (0x7fffffff) argument" do
      v = 0xdeadbeef
      called = false
      LibTest.testAsyncCallback(0x7fffffff) { |i| v = i; called = true }
      called.should be_true
      v.should == 0x7fffffff
    end
  end
end