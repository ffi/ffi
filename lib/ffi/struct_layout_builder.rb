#
# Copyright (C) 2008-2010 Wayne Meissner
#
# All rights reserved.
#
# This file is part of ruby-ffi.
#
# This code is free software: you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License version 3 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
# version 3 for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# version 3 along with this work.  If not, see <http://www.gnu.org/licenses/>.
#

module FFI
  
  # Build a {StructLayout struct layout}.
  class StructLayoutBuilder
    attr_reader :size
    attr_reader :alignment
    
    def initialize
      @size = 0
      @alignment = 1
      @min_alignment = 1
      @packed = false
      @union = false
      @fields = Array.new
    end

    # @param [Numeric] size
    # Set size attribute with +size+ only if +size+ is greater than attribute value.
    def size=(size)
      @size = size if size > @size
    end

    # @param [Numeric] alignment
    # Set alignment attribute with +alignment+ only if it is greater than attribute value.
    def alignment=(align)
      @alignment = align if align > @alignment
      @min_alignment = align
    end

    # @param [Boolean] is_union
    # @return [is_union]
    # Set union attribute.
    # Set to +true+ to build a {Union} instead of a {Struct}.
    def union=(is_union)
      @union = is_union
    end

    # @return [Boolean]
    # Building a {Union} or a {Struct} ?
    def union?
      @union
    end

    # Set packed attribute
    # @overload packed=(packed)
    #  @param [Fixnum] packed
    #  @return [packed]
    #  Set alignment and packed attributes to +packed+.
    # @overload packed=(packed)
    #  @param packed
    #  @return [0,1]
    #  Set packed attribute.
    def packed=(packed)
      if packed.is_a?(Fixnum)
        @alignment = packed
        @packed = packed
      else
        @packed = packed ? 1 : 0
      end
    end


    # List of number types
    NUMBER_TYPES = [
      Type::INT8,
      Type::UINT8,
      Type::INT16,
      Type::UINT16,
      Type::INT32,
      Type::UINT32,
      Type::LONG,
      Type::ULONG,
      Type::INT64,
      Type::UINT64,
      Type::FLOAT32,
      Type::FLOAT64,
      Type::LONGDOUBLE,
      Type::BOOL,
    ]

    # @param [String, Symbol] name name of the field
    # @param [Array, DataConverter, Struct, StructLayout::Field, Symbol, Type] type type of the field
    # @param [Numeric, nil] offset
    # @return [self]
    # Add a field to the builder.
    # @note Setting +offset+ to +nil+ or +-1+ is equivalent to +0+.
    def add(name, type, offset = nil)

      if offset.nil? || offset == -1
        offset = @union ? 0 : align(@size, @packed ? [ @packed, type.alignment ].min : [ @min_alignment, type.alignment ].max)
      end

      #
      # If a FFI::Type type was passed in as the field arg, try and convert to a StructLayout::Field instance
      #
      field = type.is_a?(StructLayout::Field) ? type : field_for_type(name, offset, type)
      @fields << field
      @alignment = [ @alignment, field.alignment ].max unless @packed
      @size = [ @size, field.size + (@union ? 0 : field.offset) ].max

      return self
    end

    # @param (see #add)
    # @return (see #add)
    # Same as {#add}.
    # @see #add
    def add_field(name, type, offset = nil)
      add(name, type, offset)
    end
    
    # @param (see #add)
    # @return (see #add)
    # Add a struct as a field to the builder.
    def add_struct(name, type, offset = nil)
      add(name, Type::Struct.new(type), offset)
    end

    # @param name (see #add)
    # @param type (see #add)
    # @param [Numeric] count array length
    # @param offset (see #add)
    # @return (see #add)
    # Add an array as a field to the builder.
    def add_array(name, type, count, offset = nil)
      add(name, Type::Array.new(type, count), offset)
    end

    # @return [StructLayout]
    # Build and return the struct layout.
    def build
      # Add tail padding if the struct is not packed
      size = @packed ? @size : align(@size, @alignment)
      
      layout = StructLayout.new(@fields, size, @alignment)
      layout.__union! if @union
      layout
    end

    private
    
    # @param [Numeric] offset
    # @param [Numeric] align
    # @return [Numeric]
    def align(offset, align)
      align + ((offset - 1) & ~(align - 1));
    end

    # @param (see #add)
    # @return [StructLayout::Field]
    def field_for_type(name, offset, type)
      field_class = case
      when type.is_a?(Type::Function)
        StructLayout::Function

      when type.is_a?(Type::Struct)
        StructLayout::InnerStruct

      when type.is_a?(Type::Array)
        StructLayout::Array

      when type.is_a?(FFI::Enum)
        StructLayout::Enum

      when NUMBER_TYPES.include?(type)
        StructLayout::Number

      when type == Type::POINTER
        StructLayout::Pointer

      when type == Type::STRING
        StructLayout::String

      when type.is_a?(Class) && type < StructLayout::Field
        type

      when type.is_a?(DataConverter)
        return StructLayout::Mapped.new(name, offset, Type::Mapped.new(type), field_for_type(name, offset, type.native_type))

      when type.is_a?(Type::Mapped)
        return StructLayout::Mapped.new(name, offset, type, field_for_type(name, offset, type.native_type))

      else
        raise TypeError, "invalid struct field type #{type.inspect}"
      end

      field_class.new(name, offset, type)
    end
  end

end
