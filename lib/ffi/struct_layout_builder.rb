
module FFI
  class StructLayoutBuilder
    attr_reader :size, :alignment
    
    def initialize
      @size = 0
      @alignment = 1
      @min_alignment = 1
      @packed = false
      @union = false
      @field_names = Array.new
      @field_map = Hash.new
    end

    def size=(size)
      @size = size if size > @size
    end

    def alignment=(align)
      @alignment = align if align > @alignment
      @min_alignment = align
    end

    def union=(is_union)
      @union = is_union
    end

    def union?
      @union
    end

    def packed=(packed)
      if packed.is_a?(Fixnum)
        @alignment = packed
        @packed = packed
      else
        @packed = packed ? 1 : 0
      end
    end


    def add_field(name, type, offset = nil)

      if offset.nil? || offset == -1
        offset = @union ? 0 : align(@size, @packed ? @packed : [ @min_alignment, type.alignment ].max)
      end

      #
      # If a FFI::Type type was passed in as the field arg, try and convert to a StructLayout::Field instance
      #
      field = if !type.is_a?(StructLayout::Field)

        field_class = if type.is_a?(FFI::Type::Function)
          StructLayout::Function

        elsif type.is_a?(FFI::Type::Struct)
          StructLayout::InnerStruct

        elsif type.is_a?(FFI::Type::Array)
          StructLayout::Array

        elsif type.is_a?(FFI::Enum)
          StructLayout::Enum

        elsif type.is_a?(FFI::Type)
          StructLayout::Field

        else
          raise TypeError, "invalid struct field type #{type.inspect}" unless field_class
        end

        field_class.new(name, offset, type)

      else
        type
      end

      store_field(name, field, type)
      
    end

    def add_struct(name, type, offset = nil)
      add_field(name, FFI::Type::Struct.new(type), offset)
    end

    def add_array(name, type, count, offset = nil)
      add_field(name, FFI::Type::Array.new(type, count), offset)
    end

    def build
      # Add tail padding if the struct is not packed
      size = @packed ? @size : align(@size, @alignment)
      
      FFI::StructLayout.new(@field_names, @field_map, size, @alignment)
    end

    private
    
    def align(offset, align)
      align + ((offset - 1) & ~(align - 1));
    end


    def store_field(name, field, type)
      @field_names << name
      @field_map[name] = field
      @alignment = @packed ? @packed : [ @alignment, type.alignment ].max
      @size = [ @size, type.size + (@union ? 0 : field.offset) ].max
    end

  end

end