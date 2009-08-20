require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

describe FFI::Function do
  module LibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH
    attach_function :testFunctionAdd, [:int, :int, :pointer], :int
    attach_function :testAddPointer, [], :pointer
  end
  it 'is initialized with a signature and a block' do
    FFI::Function.new(:int, []) { }
  end
  it 'raises an error when passing a wrong signature' do
    lambda { FFI::Function.new([], :int).new { } }.should raise_error TypeError 
  end
  it 'returns a native pointer' do
    FFI::Function.new(:int, []) { }.kind_of? FFI::Pointer
  end
  it 'can be used as callback from C passing to it a block' do
    function_add = FFI::Function.new(:int, [:int, :int]) { |a, b| a + b }
    LibTest.testFunctionAdd(10, 10, function_add).should == 20
  end
  it 'can be used as callback from C passing to it a Proc object' do
    function_add = FFI::Function.new(:int, [:int, :int], Proc.new { |a, b| a + b })
    LibTest.testFunctionAdd(10, 10, function_add).should == 20
  end
  it 'can be used to wrap an existing function pointer' do
    FFI::Function.new(:int, [:int, :int], LibTest.testAddPointer).call(10, 10).should == 20
  end
  it 'can be attached to a module' do
    module Foo; end
    fp = FFI::Function.new(:int, [:int, :int], LibTest.testAddPointer)
    fp.attach(Foo, 'add')
    Foo.add(10, 10).should == 20
  end
  it 'can be used to extend an object' do
    fp = FFI::Function.new(:int, [:int, :int], LibTest.testAddPointer)
    foo = Object.new
    class << foo
      def singleton_class
        class << self; self; end
      end
    end
    fp.attach(foo.singleton_class, 'add')
    foo.add(10, 10).should == 20    
  end
end
