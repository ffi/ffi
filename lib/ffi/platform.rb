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

require 'rbconfig'
module FFI
  class PlatformError < LoadError; end

  # This module defines different constants and class methods to play with
  # various platforms.
  module Platform
    OS = case RbConfig::CONFIG['host_os'].downcase
    when /linux/
      "linux"
    when /darwin/
      "darwin"
    when /freebsd/
      "freebsd"
    when /openbsd/
      "openbsd"
    when /sunos|solaris/
      "solaris"
    when /mingw|mswin/
      "windows"
    else
      RbConfig::CONFIG['host_os'].downcase
    end

    ARCH = case CPU.downcase
    when /amd64|x86_64/
      "x86_64"
    when /i?86|x86|i86pc/
      "i386"
    when /ppc|powerpc/
      "powerpc"
    else
      case RbConfig::CONFIG['host_cpu']
      when /^arm/
        "arm"
      else
        RbConfig::CONFIG['host_cpu']
      end
    end

    private
    # @param [String) os
    # @return [Boolean]
    # Test if current OS is +os+.
    def self.is_os(os)
      OS == os
    end
    
    NAME = "#{ARCH}-#{OS}"
    IS_GNU = defined?(GNU_LIBC)
    IS_LINUX = is_os("linux")
    IS_MAC = is_os("darwin")
    IS_FREEBSD = is_os("freebsd")
    IS_OPENBSD = is_os("openbsd")
    IS_WINDOWS = is_os("windows")
    IS_BSD = IS_MAC || IS_FREEBSD || IS_OPENBSD
    CONF_DIR = File.join(File.dirname(__FILE__), 'platform', NAME)
    public

    

    LIBPREFIX = IS_WINDOWS ? '' : 'lib'

    LIBSUFFIX = case OS
    when /darwin/
      'dylib'
    when /linux|bsd|solaris/
      'so'
    when /windows/
      'dll'
    else
      # Punt and just assume a sane unix (i.e. anything but AIX)
      'so'
    end

    LIBC = if IS_WINDOWS
      RbConfig::CONFIG['RUBY_SO_NAME'].split('-')[-2] + '.dll'
    elsif IS_GNU
      GNU_LIBC
    else
      "#{LIBPREFIX}c.#{LIBSUFFIX}"
    end

    # Test if current OS is a *BSD (include MAC)
    # @return [Boolean]
    def self.bsd?
      IS_BSD
    end

    # Test if current OS is Windows
    # @return [Boolean]
    def self.windows?
      IS_WINDOWS
    end

    # Test if current OS is Mac OS
    # @return [Boolean]
    def self.mac?
      IS_MAC
    end

    # Test if current OS is a unix OS
    # @return [Boolean]
    def self.unix?
      !IS_WINDOWS
    end
  end
end

