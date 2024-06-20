#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#

require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

require "ffi/tools/generator"

# We use `SPEC_NUMBER_PLACEHOLDER`, since the generated values are
# platform-spec.
describe "Generator" do
  include FFI

  let(:input_file)  { Tempfile.new 'zlib.rb.ffi' }
  let(:output_file) { Tempfile.new 'zlib.rb' }

  before :each do
    input_file.puts <<~FILE
      module Zlib
        @@@
        constants do |c|
          c.include "zlib.h"
          c.const :ZLIB_VERNUM
        end
        @@@

        class ZStream < FFI::Struct

          @@@
          struct do |s|
            s.name "struct z_stream_s"
            s.include "zlib.h"

            s.field :next_in,   :pointer
            s.field :avail_in,  :uint
            s.field :total_in,  :ulong
          end
          @@@

        end
      end
    FILE
    input_file.rewind
  end

  let :expected_output do
    <<~FILE
      # This file is generated from `#{input_file.to_path}'. Do not edit.

      module Zlib
        ZLIB_VERNUM = SPEC_NUMBER_PLACEHOLDER





        class ZStream < FFI::Struct

          layout :next_in, :pointer, SPEC_NUMBER_PLACEHOLDER,
                 :avail_in, :uint, SPEC_NUMBER_PLACEHOLDER,
                 :total_in, :ulong, SPEC_NUMBER_PLACEHOLDER







        end
      end
    FILE
  end

  after :each do
    [input_file, output_file].each do |f|
      f.close
      f.unlink
    end
  end

  it "Generates FFI::Structs and C constants from a template file" do
    FFI::Generator.new input_file.to_path, output_file.to_path
    output_contents = File.read(output_file)
                          .gsub(/ \d+/, ' SPEC_NUMBER_PLACEHOLDER')
    expect(expected_output).to eq(output_contents)
  end
end
