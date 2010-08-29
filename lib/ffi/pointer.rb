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
    # Write +obj+ as a C int at the memory pointed to.
    def write_int(obj)
      put_int32(0, obj)
    end

    # Read a C int from the memory pointed to.
    def read_int
      get_int32(0)
    end

    # Write +obj+ as a C long at the memory pointed to.
    def write_long(obj)
      put_long(0, obj)
    end

    # Read a C long from the memory pointed to.
    def read_long
      get_long(0)
    end
    # Write +obj+ as a C long long at the memory pointed to.
    def write_long_long(obj)
      put_int64(0, obj)
    end

    # Read a C long long from the memory pointed to.
    def read_long_long
      get_int64(0)
    end

    def read_pointer
      get_pointer(0)
    end
    def write_pointer(ptr)
      put_pointer(0, ptr)
    end

    def read_float
      get_float32(0)
    end
    def write_float(obj)
      put_float32(0, obj)
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
    def read_array_of_int(length)
      get_array_of_int32(0, length)
    end

    def write_array_of_int(ary)
      put_array_of_int32(0, ary)
    end

    def read_array_of_long(length)
      get_array_of_long(0, length)
    end

    def write_array_of_long(ary)
      put_array_of_long(0, ary)
    end

    def read_array_of_pointer(length)
      read_array_of_type(:pointer, :read_pointer, length)
    end

    def write_array_of_pointer(ary)
      write_array_of_type(:pointer, :write_pointer, ary)
    end

  end
end
