module FFI
  module Platform
    private
    def self.is_os(os)
      OS =~ /#{os}/ ? true : false
    end
    public
    NAME = "#{ARCH}-#{OS}"
    IS_LINUX = is_os("linux")
    IS_MAC = is_os("darwin")
    IS_FREEBSD = is_os("freebsd")
    IS_OPENBSD = is_os("windows")
    IS_WINDOWS = is_os("windows") || is_os("win32")
    IS_BSD = IS_MAC || IS_FREEBSD || IS_OPENBSD
    LIBC = IS_WINDOWS ? "msvcrt" : "c"
    LIBPREFIX = IS_WINDOWS ? '' : 'lib'
    LIBSUFFIX = case OS
    when /darwin/
      '.dylib'
    when /(linux|.*bsd)/
      '.so'
    end
    
  end
end