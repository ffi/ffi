require 'ffi/struct'

module FFI
  class UnionLayoutBuilder < FFI::StructLayoutBuilder
    private
    def initialize
      self.union = true
    end
    def calc_alignment_of(field_class, offset); 0; end
  end
  class Union < FFI::Struct
    private
    def self.builder
      UnionLayoutBuilder.new
    end
  end
end
