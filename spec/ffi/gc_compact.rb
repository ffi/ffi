# -*- encoding: utf-8 -*-
#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#

require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

# These specs are not run as regular rspec file, but as a separate process from string_spec.rb.
# That's due to GC.verify_compaction_references is very memory consuming since it doubles the needed memory with each call.
# So it's intentionally that the file name miss the "_spec" prefix.

if GC.respond_to?(:compact)
  describe "GC.compact" do
    module GcCompactTestLib
      extend FFI::Library
      ffi_lib TestLibrary::PATH
      attach_function :store_string, [ :string, :pointer ], :void
      attach_function :store_pointer, :store_string, [ :pointer, :pointer ], :void
    end

    A = []
    F = []
    [0, 1, 23, 24].each_with_index do |len, idx|
      A[idx] = "a" * len
      F[idx] = ("a" * len).freeze

      it "preserves a #{len} character string" do
        mem = FFI::MemoryPointer.new :pointer, 3
        GcCompactTestLib.store_string(A[idx], mem)
        GcCompactTestLib.store_pointer(A[idx], mem + 1*FFI::TYPE_POINTER.size)
        GcCompactTestLib.store_string(F[idx], mem + 2*FFI::TYPE_POINTER.size)

        GC.verify_compaction_references(toward: :empty, double_heap: true)

        expect( mem.get_pointer(0*FFI::TYPE_POINTER.size).read_string ).to eq("a" * len)
        expect( mem.get_pointer(1*FFI::TYPE_POINTER.size).read_string ).to eq("a" * len)
        expect( mem.get_pointer(2*FFI::TYPE_POINTER.size).read_string ).to eq("a" * len)
        expect( A[idx] ).to eq("a" * len)
        expect( F[idx] ).to eq("a" * len)
      end
    end
  end
end
