module FFI
  module IO
    def self.for_fd(fd, mode)
      ::IO.for_fd(fd, mode)
    end
  end
end