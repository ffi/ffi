#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#

library_path = ARGV.fetch(0)
mode = ARGV.fetch(1)
library = nil

check_unloaded = proc do
  4.times { GC.start(full_mark: true, immediate_sweep: true) }

  begin
    FFI::DynamicLibrary.open(
      library_path,
      FFI::DynamicLibrary::RTLD_LAZY | FFI::DynamicLibrary::RTLD_NOLOAD,
    )
  rescue LoadError
    exit 42
  end

  exit 43
end

if mode == "at_exit"
  # Register before loading ffi so this runs after any END proc ffi installs.
  at_exit do
    library = nil
    check_unloaded.call
  end
end

require 'ffi'

library = Module.new do
  extend FFI::Library
  ffi_lib library_path
end

if mode == "runtime"
  library = nil
  check_unloaded.call
end
