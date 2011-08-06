#
# Copyright (C) 2008, 2009 Wayne Meissner
# Copyright (c) 2007, 2008 Evan Phoenix
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

require 'ffi/platform'
module FFI
  class Pointer
    SIZE = Platform::ADDRESS_SIZE / 8

    # Return the size of a pointer on the current platform, in bytes
    def self.size
      SIZE
    end

    def read_string(len=nil)
      if len
        get_bytes(0, len)
      else
        get_string(0)
      end
    end

    def read_string_length(len)
      get_bytes(0, len)
    end

    def read_string_to_null
      get_string(0)
    end

    def write_string_length(str, len)
      put_bytes(0, str, 0, len)
    end

    def write_string(str, len=nil)
      len = str.bytesize unless len
      # Write the string data without NUL termination
      put_bytes(0, str, 0, len)
    end

    def read_array_of_type(type, reader, length)
      ary = []
      size = FFI.type_size(type)
      tmp = self
      length.times { |j|
        ary << tmp.send(reader)
        tmp += size unless j == length-1 # avoid OOB
      }
      ary
    end

    def write_array_of_type(type, writer, ary)
      size = FFI.type_size(type)
      tmp = self
      ary.each_with_index {|i, j|
        tmp.send(writer, i)
        tmp += size unless j == ary.length-1 # avoid OOB
      }
      self
    end

    def typecast (type)
      type = FFI.find_type(type) if type.is_a?(Symbol)

      if type.is_a?(Class) && type.ancestors.member?(FFI::Struct) && !type.is_a?(FFI::ManagedStruct)
        type.new(self)
      elsif type.is_a?(Type::Builtin)
        send "read_#{type.name.downcase}"
      elsif type.respond_to? :from_native
        type.from_native(typecast(type.native_type), nil)
      else
        raise ArgumentError, 'you have to pass a Struct, a Builtin type or a Symbol'
      end
    end

    def read_array_of (type, number)
      type = FFI.find_type(type) if type.is_a? Symbol
      type = type.native_type    if type.respond_to? :native_type

      unless respond_to? "read_array_of_#{type.name.downcase}"
        raise ArgumentError, "can't read array of #{type.name}"
      end

      send "read_array_of_#{type.name.downcase}", number
    end
  end
end
