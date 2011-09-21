#
# Copyright (C) 2008, 2009 Wayne Meissner
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
  
  # This module implements a couple of class methods to play with IO.
  module IO
    # @param [Integer] fd file decriptor
    # @param [String] mode mode string
    # @return [::IO]
    # Synonym for IO::for_fd.
    def self.for_fd(fd, mode = "r")
      ::IO.for_fd(fd, mode)
    end

    # @param [#read] io io to read from
    # @param [AbstractMemory] buf destination for data read from +io+
    # @param [nil, Numeric] len maximul number of bytes to read from +io+. If +nil+, 
    #  read until end of file.
    # @return [Numeric] length really read, in bytes
    #
    # A version of IO#read that reads data from an IO and put then into a native buffer.
    # 
    # This will be optimized at some future time to eliminate the double copy.
    #
    def self.native_read(io, buf, len)
      tmp = io.read(len)
      return -1 unless tmp
      buf.put_bytes(0, tmp)
      tmp.length
    end

  end
end

