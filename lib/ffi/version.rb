#
# Copyright (C) 2008, 2009 Wayne Meissner
# All rights reserved.
#
# This file is part of ruby-ffi.
#
# All rights reserved.
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

##
# This is a simplified version of Gem::Version.  Besides removing
# code, this version handles nil differently.  A nil version
# is considered greater than any other version.  The reason for
# this is that when a library has a nil version it should be
# assumed to contain a function.  Thus Versin(nil) > Version('1.2.3').

module FFI
  class Version
    include Comparable

    VERSION_PATTERN = '[0-9]+(\.[0-9a-zA-Z]+)*' # :nodoc:

    # A string representation of this Version.
    attr_reader :version
    alias to_s version

    # Factory method to create a Version object. Input may be a Version
    # or a String. Intended to simplify client code.
    #
    #   ver1 = Version.create('1.3.17')   # -> (Version object)
    #   ver2 = Version.create(ver1)       # -> (ver1)
    #   ver3 = Version.create(nil)        # -> (nil)

    def self.create input
      if input.respond_to? :version then
        input
      else
        new input
      end
    end

    # Constructs a Version from the +version+ string.  A version string is a
    # series of digits or ASCII letters separated by dots.
    def initialize version
      @version = version.nil? ? nil : version.to_s.strip
    end

    # A Version is only eql? to another version if it's specified to the
    # same precision. Version "1.0" is not the same as version "1".
    def eql? other
      self.class === other and @version == other.version
    end

    def segments
      # segments is lazy so it can pick up version values that come from
      # old marshaled versions, which don't go through marshal_load.

      @segments ||= @version.scan(/[0-9]+|[a-z]+/i).map do |s|
        /^\d+$/ =~ s ? s.to_i : s
      end
    end

    # Compares this version with +other+ returning -1, 0, or 1 if the
    # other version is larger, the same, or smaller than this
    # one. Attempts to compare to something that's not a
    # <tt>Gem::Version</tt> return +nil+.

    def <=> other
      return unless FFI::Version === other
      return 0 if @version == other.version
      return 1 if @version.nil?
      return -1 if other.version.nil?

      lhsegments = segments
      rhsegments = other.segments

      lhsize = lhsegments.size
      rhsize = rhsegments.size
      limit  = (lhsize > rhsize ? lhsize : rhsize) - 1

      i = 0

      while i <= limit
        lhs, rhs = lhsegments[i] || 0, rhsegments[i] || 0
        i += 1

        next      if lhs == rhs
        return -1 if String  === lhs && Numeric === rhs
        return  1 if Numeric === lhs && String  === rhs

        return lhs <=> rhs
      end

      return 0
    end
  end
end