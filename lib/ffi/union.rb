require 'ffi/struct'

module FFI
  class UnionLayoutBuilder < FFI::StructLayoutBuilder
    def add_field(name, type, offset = nil)
      field_class, info = field_class_from(type)
      size = field_class.size / 8
      @fields[name] = field_class.new(0, info)
      @size = size if size > @size
      @min_align = field_class.align if field_class.align > @min_align
    end
  end
  class Union < FFI::Struct
    def self.builder
      UnionLayoutBuilder.new
    end
  end
end
