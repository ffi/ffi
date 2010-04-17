
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


    NUMBER_TYPES = [
      FFI::Type::INT8,
      FFI::Type::UINT8,
      FFI::Type::INT16,
      FFI::Type::UINT16,
      FFI::Type::INT32,
      FFI::Type::UINT32,
      FFI::Type::LONG,
      FFI::Type::ULONG,
      FFI::Type::INT64,
      FFI::Type::UINT64,
      FFI::Type::FLOAT32,
      FFI::Type::FLOAT64,
    ]

    def add_field(name, type, offset = nil)

      if offset.nil? || offset == -1
        offset = @union ? 0 : align(@size, @packed ? @packed : [ @min_alignment, type.alignment ].max)
      end

      #
      # If a FFI::Type type was passed in as the field arg, try and convert to a StructLayout::Field instance
      #
      field = if !type.is_a?(StructLayout::Field)

        field_class = case
          when type.is_a?(FFI::Type::Function)
            StructLayout::Function

          when type.is_a?(FFI::Type::Struct)
            StructLayout::InnerStruct

          when type.is_a?(FFI::Type::Array)
            StructLayout::Array

          when type.is_a?(FFI::Enum)
            StructLayout::Enum

          when NUMBER_TYPES.include?(type)
            StructLayout::Number

          when type == FFI::Type::POINTER
            StructLayout::Pointer

          when type == FFI::Type::STRING
            StructLayout::String

          else
            raise TypeError, "invalid struct field type #{type.inspect}"
          end

        field_class.new(name, offset, type)

      else
        type
      end

      store_field(name, field)
      
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


    def store_field(name, field)
      @field_names << name
      @field_map[name] = field
      @alignment = @packed ? @packed : [ @alignment, field.alignment ].max
      @size = [ @size, field.size + (@union ? 0 : field.offset) ].max
    end

  end

end