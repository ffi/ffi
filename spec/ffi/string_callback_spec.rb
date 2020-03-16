#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#

require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

describe "callback with return value" do
  module LibTest
    extend FFI::Library
    ffi_lib TestLibrary::PATH

    callback :greeter, [:string], :string

    @blocking = true
    attach_function :testCallback, [ :greeter, :string ], :string
  end

  it "accepts memory pointer with string" do
    result = LibTest.testCallback("jack") do |name|
      FFI::MemoryPointer.from_string("hello #{name}")
    end
    expect(result).to eq("hello jack")
  end
end
