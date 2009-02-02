require 'ffi/struct'

module FFI
  class UnionLayoutBuilder < FFI::StructLayoutBuilder
    private
    def calc_alignment_of(field_class, offset); 0; end
    def calc_current_size(offset, size)
      @size = size if size > @size
    end
  end
  class Union < FFI::Struct
    private
    def self.builder
      UnionLayoutBuilder.new
    end
  end
end
