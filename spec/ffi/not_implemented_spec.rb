#
# This file is part of ruby-ffi.
#
# This code is free software: you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License version 3 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
# version 3 for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# version 3 along with this work.  If not, see <http://www.gnu.org/licenses/>.
#

require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

describe FFI::NotImplemented do
  it 'is initialized with a cname, required version and current version' do
    FFI::NotImplemented.new('some_fuction', FFI::Version.new(nil), FFI::Version.new(nil))
  end

  it 'returns a NotImplemented instance' do
    FFI::NotImplemented.new('some_fuction', FFI::Version.new(nil), FFI::Version.new(nil)).kind_of? FFI::NotImplemented
  end

  it 'raises an error when invoked' do
    method = FFI::NotImplemented.new('some_fuction', FFI::Version.new('1.2.3'), FFI::Version.new('1.1.0'))
    expect do
      method.call
    end.to raise_error(RuntimeError, "The function some_fuction is only available in version 1.2.3 and higher. You are running version 1.1.0")
  end

  it 'can be attached to a module' do
    module Foo; end
    fp = FFI::NotImplemented.new('some_fuction', FFI::Version.new('1.2.3'), FFI::Version.new('1.1.0'))
    fp.attach(Foo, 'add')
    expect do
      Foo.add(10, 10).should == 20
    end.to raise_error(RuntimeError, "The function some_fuction is only available in version 1.2.3 and higher. You are running version 1.1.0")
  end

  it 'can be used to extend an object' do
    fp = FFI::NotImplemented.new('some_fuction', FFI::Version.new('1.2.3'), FFI::Version.new('1.1.0'))
    foo = Object.new
    class << foo
      def singleton_class
        class << self; self; end
      end
    end
    fp.attach(foo.singleton_class, 'add')

    expect do
      foo.add(10, 10).should == 20
    end.to raise_error(RuntimeError, "The function some_fuction is only available in version 1.2.3 and higher. You are running version 1.1.0")
  end
end

describe "Library version is too old" do
  before do
    module LibTest
      extend FFI::Library
      libs = ffi_lib TestLibrary::PATH
      libs.first.version = FFI::Version.new('1.1.0')
      attach_function :testFunctionAdd, [:int, :int, :pointer], :int, :version => '1.2.3'
    end
  end

  it 'raises an error when invoked' do
    expect do
      function_add = FFI::Function.new(:int, [:int, :int]) { |a, b| a + b }
      LibTest.testFunctionAdd(10, 10, function_add).should == 20
    end.to raise_error(RuntimeError, "The function testFunctionAdd is only available in version 1.2.3 and higher. You are running version 1.1.0")
  end
end

describe "Library version is good" do
  before do
    module LibTest
      extend FFI::Library
      libs = ffi_lib TestLibrary::PATH
      libs.first.version = FFI::Version.new('1.2.3')
      attach_function :testFunctionAdd, [:int, :int, :pointer], :int, :version => '1.1.1'
    end
  end

  it 'raises an error when invoked' do
    function_add = FFI::Function.new(:int, [:int, :int]) { |a, b| a + b }
    LibTest.testFunctionAdd(10, 10, function_add).should == 20
  end
end
