#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#
# Run in a fresh process because RTLD_NODELETE permanently tags a loaded image.
#

require "ffi"

library_path = ARGV.fetch(0)
library = FFI::DynamicLibrary.open(
  library_path,
  FFI::DynamicLibrary::RTLD_LAZY | FFI::DynamicLibrary::RTLD_LOCAL,
)
library = nil
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
