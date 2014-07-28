#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#

require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))
require 'ffi'

describe "FFI::VERSION" do
  subject { FFI::VERSION }
  it "should return a string of the current FFI version" do
    expect(FFI::VERSION).to match("1.9.3")
  end
end
