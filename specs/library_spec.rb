require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))
describe "Library" do
  it "attach_function with no library specified" do
    lambda {
      Module.new do |m|
        m.extend FFI::Library
        attach_function :getpid, [ ], :uint
      end
    }.should_not raise_error
  end
  it "attach_function :getpid from this process" do
    lambda {
      Module.new do |m|
        m.extend FFI::Library
        attach_function :getpid, [ ], :uint
      end.getpid.should == Process.pid
    }.should_not raise_error
  end
  it "attach_function :getpid from [ 'c', 'libc.so.6'] " do
    lambda {
      Module.new do |m|
        m.extend FFI::Library
        ffi_lib 'c', 'libc.so.6'
        attach_function :getpid, [ ], :uint
      end.getpid.should == Process.pid
    }.should_not raise_error
  end
  it "attach_function :getpid from [ 'libc.so.6', 'c' ] " do
    lambda {
      Module.new do |m|
        m.extend FFI::Library
        ffi_lib 'libc.so.6', 'c'
        attach_function :getpid, [ ], :uint
      end.getpid.should == Process.pid
    }.should_not raise_error
  end
  it "attach_function :getpid from [ 'libfubar.so.0xdeadbeef', nil, 'c' ] " do
    lambda {
      Module.new do |m|
        m.extend FFI::Library
        ffi_lib 'libfubar.so.0xdeadbeef', nil, 'c'
        attach_function :getpid, [ ], :uint
      end.getpid.should == Process.pid
    }.should_not raise_error
  end
  it "attach_function :getpid from [ 'libfubar.so.0xdeadbeef' ] " do
    lambda {
      Module.new do |m|
        m.extend FFI::Library
        ffi_lib 'libfubar.so.0xdeadbeef'
        attach_function :getpid, [ ], :uint
      end.getpid.should == Process.pid
    }.should raise_error(LoadError)
  end
end