#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#
# Run by library_unload_spec.rb in a child process, because the failure it looks for is
# a fatal signal. Loads the fixture through ffi_lib, starts threads inside it, then
# either drops the module or holds it and exits normally. Exits 42 if every thread
# started, 43 otherwise.
#

require 'ffi'

mode, library_path = ARGV

library = Module.new do
  extend FFI::Library
  ffi_lib library_path
  attach_function :startDetachedThreads, [:int], :int
end

exit 43 unless library.startDetachedThreads(4) == 4

case mode
when "collect"
  # Nothing references the module or its library any more, so the next GC frees them.
  library = nil
  4.times { GC.start(full_mark: true, immediate_sweep: true) }
  sleep 0.05
when "shutdown"
  # The module stays referenced. Ruby's shutdown sweep frees its library anyway.
else
  raise ArgumentError, "unknown mode #{mode.inspect}"
end

# "collect" has already been proven by this point, so leave without a shutdown sweep.
# "shutdown" needs the sweep, which is what exiting normally runs.
exit!(42) if mode == "collect"
exit(42)
