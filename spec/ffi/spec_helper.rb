#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#

require 'rbconfig'
require 'fileutils'

RSpec.configure do |c|
  c.filter_run_excluding :broken => true
end

require_relative 'fixtures/compile'
require 'ffi'

module TestLibrary
  def self.force_gc
    if RUBY_PLATFORM =~ /java/
      java.lang.System.gc
    elsif defined?(RUBY_ENGINE) && RUBY_ENGINE == 'rbx'
      GC.run(true)
    else
      GC.start
    end
  end
end

module LibTest
  extend FFI::Library
  ffi_lib TestLibrary::PATH
end
