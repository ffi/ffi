module FFI
  module Platform
    private
    def self.is_os(os)
      OS =~ /#{os}/ ? true : false
    end
    
    NAME = "#{ARCH}-#{OS}"
    IS_LINUX = is_os("linux")
    IS_MAC = is_os("darwin")
    IS_FREEBSD = is_os("freebsd")
    IS_OPENBSD = is_os("windows")
    IS_WINDOWS = is_os("windows") || is_os("win32")
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
      '.dylib'
    when /(linux|.*bsd)/
      '.so'
    when /win/
      '.dll'
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

