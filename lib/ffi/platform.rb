require 'rbconfig'
module FFI
  class PlatformError < FFI::NativeError; end

  module Platform
    OS = case Config::CONFIG['host_os'].downcase
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
    when /win/
      "windows"
    else
      raise FFI::PlatformError, "Unknown operating system: #{OS_}"
    end
    ARCH = case CPU.downcase
    when /i?86|x86|i86pc/
      "i386"
    when /amd64|x86_64/
      "x86_64"
    when /ppc|powerpc/
      "powerpc"
    else
      raise FFI::PlatformError, "Unknown cpu architecture: #{ARCH_}"
    end
    private
    def self.is_os(os)
      OS == os
    end
    
    NAME = "#{ARCH}-#{OS}"
    IS_LINUX = is_os("linux")
    IS_MAC = is_os("darwin")
    IS_FREEBSD = is_os("freebsd")
    IS_OPENBSD = is_os("openbsd")
    IS_WINDOWS = is_os("windows")
    IS_BSD = IS_MAC || IS_FREEBSD || IS_OPENBSD
    public
    LIBC = if IS_WINDOWS
      "msvcrt"
    elsif IS_LINUX
      "libc.so.6"
    else
      "c"
    end
    LIBPREFIX = IS_WINDOWS ? '' : 'lib'
    LIBSUFFIX = case OS
    when /darwin/
      'dylib'
    when /linux|bsd|solaris/
      'so'
    when /win/
      'dll'
    else
      raise PlatformError, "Cannot determine shared library extension for #{OS}"
    end
    def self.bsd?
      IS_BSD
    end
    def self.windows?
      IS_WINDOWS
    end
    def self.mac?
      IS_MAC
    end
    def self.unix?
      !IS_WINDOWS
    end
  end
end

