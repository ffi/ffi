#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#
# Run by library_unload_spec.rb in a child process, because the failure it looks for is
# a fatal signal. Loads the fixture through ffi_lib, starts threads inside it, then exits
# normally so Ruby's shutdown sweep frees the still-referenced library.
#

require 'ffi'

library_path = ARGV.fetch(0)

library = Module.new do
  extend FFI::Library
  # Explicit unloadable flags ensure this exercises library_free's shutdown guard.
  ffi_lib_flags :lazy, :local
  ffi_lib library_path
  attach_function :startDetachedThreads, [:int], :int
end

exit 43 unless library.startDetachedThreads(4) == 4
exit(42)
