module FFI
  # This module is used to extend somes classes and give then a common API.
  #
  # Most of methods defined here must be overriden.
  module DataConverter
    # Get native type.
    #
    # @overload native_type(type)
    #  @param [String, Symbol, Type] type
    #  @return [Type]
    #  Get native type from +type+.
    #
    # @overload native_type
    #  @raise {NotImplementedError} This method must be overriden.
    def native_type(type = nil)
      if type
        @native_type = FFI.find_type(type)
      else
        native_type = @native_type
        unless native_type
          raise NotImplementedError, 'native_type method not overridden and no native_type set'
        end
        native_type
      end
    end

    # Convert to a native type.
    def to_native(value, ctx)
      value
    end

    # Convert from a native type.
    def from_native(value, ctx)
      value
    end
  end
end
