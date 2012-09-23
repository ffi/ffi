#
# Copyright (C) 2009, 2010 Wayne Meissner
# Copyright (C) 2009 Luc Heinrich
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

  # An instance of this class permits to manage {Enum}s. In fact, Enums is a collection of {Enum}s.
  class Enums

    # @return [nil]
    def initialize
      @all_enums = Array.new
      @tagged_enums = Hash.new
      @symbol_map = Hash.new
    end

    # @param [Enum] enum
    # Add an {Enum} to the collection.
    def <<(enum)
      @all_enums << enum
      @tagged_enums[enum.tag] = enum unless enum.tag.nil?
      @symbol_map.merge!(enum.symbol_map)
    end

    # @param query enum tag or part of an enum name
    # @return [Enum]
    # Find a {Enum} in collection.
    def find(query)
      if @tagged_enums.has_key?(query)
        @tagged_enums[query]
      else
        @all_enums.detect { |enum| enum.symbols.include?(query) }
      end
    end

    # @param symbol a symbol to find in merge symbol maps of all enums.
    # @return a symbol
    def __map_symbol(symbol)
      @symbol_map[symbol]
    end

  end

  # Represents a C enum.
  #
  # For a C enum:
  #  enum fruits {
  #    apple,
  #    banana,
  #    orange,
  #    pineapple
  #  };
  # are defined this vocabulary:
  # * a _symbol_ is a word from the enumeration (ie. _apple_, by example);
  # * a _value_ is the value of a symbol in the enumeration (by example, apple has value _0_ and banana _1_).
  class Enum
    include DataConverter

    attr_reader :tag

    # @param [nil, Enumerable] info
    # @param tag enum tag
    def initialize(info, tag=nil)
      @tag = tag
      @kv_map = Hash.new
      unless info.nil?
        last_cst = nil
        value = 0
        info.each do |i|
          case i
          when Symbol
            raise ArgumentError, "duplicate enum key" if @kv_map.has_key?(i)
            @kv_map[i] = value
            last_cst = i
            value += 1
          when Integer
            @kv_map[last_cst] = i
            value = i+1
          end
        end
      end
      @vk_map = @kv_map.invert
    end

    # @return [Array] enum symbol names
    def symbols
      @kv_map.keys
    end

    # Get a symbol or a value from the enum.
    # @overload [](query)
    #  Get enum value from symbol.
    #  @param [Symbol] query
    #  @return [Integer]
    # @overload [](query)
    #  Get enum symbol from value.
    #  @param [Integer] query
    #  @return [Symbol]
    def [](query)
      case query
      when Symbol
        @kv_map[query]
      when Integer
        @vk_map[query]
      end
    end
    alias find []

    # Get the symbol map.
    # @return [Hash]
    def symbol_map
      @kv_map
    end
    
    alias to_h symbol_map
    alias to_hash symbol_map

    # Get native type of Enum
    # @return [Type::INT]
    def native_type
      Type::INT
    end

    # @param [Symbol, Integer, #to_int] val
    # @param ctx unused
    # @return [Integer] value of a enum symbol
    def to_native(val, ctx)
      @kv_map[val] || if val.is_a?(Integer)
        val
      elsif val.respond_to?(:to_int)
        val.to_int
      else
        raise ArgumentError, "invalid enum value, #{val.inspect}"
      end
    end

    # @param val
    # @return symbol name if it exists for +val+.
    def from_native(val, ctx)
      @vk_map[val] || val
    end

  end

end
