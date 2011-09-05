##
## This file is part of ruby-ffi.
##
## This code is free software: you can redistribute it and/or modify it under
## the terms of the GNU Lesser General Public License version 3 only, as
## published by the Free Software Foundation.
##
## This code is distributed in the hope that it will be useful, but WITHOUT
## ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
## FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
## version 3 for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## version 3 along with this work.  If not, see <http://www.gnu.org/licenses/>.
##
#
require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

describe "Instantiate a version" do
  it "saves the correct version" do
    FFI::Version.new('1.2.3').version.should == '1.2.3'
  end

  it "saves the correct stripped version" do
    FFI::Version.new(' 1.2.3 ').version.should == '1.2.3'
  end

  it "saves nil as empty string" do
    FFI::Version.new(nil).version.should == nil
  end
end

describe "Create a version" do
  it "returns a new version from a string" do
    FFI::Version.create('1.2.3').kind_of? FFI::Version
  end

  it "returns a new version from nil" do
    FFI::Version.create(nil).kind_of? FFI::Version
  end

  it "returns the same version from a version" do
    version = FFI::Version.create('1.2.3')
    FFI::Version.create(version).should equal(version)
  end
end

describe "Versions" do
  it "equals the same version" do
    FFI::Version.new('1.2.3').should == FFI::Version.new('1.2.3')
  end

  it "is less than a nil version" do
    FFI::Version.new('1.2.3').should < FFI::Version.new(nil)
  end
end

describe "Nil version" do
  it "equals a nil version" do
    FFI::Version.new(nil).should == FFI::Version.new(nil)
  end

  it "is greater than a version" do
    FFI::Version.new(nil).should > FFI::Version.new('1.2.3')
  end
end