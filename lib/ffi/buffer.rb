module FFI
  class Buffer
    def self.new(size, count=nil, clear=true)
      self.__alloc_inout(size, count, clear)
    end
    def self.alloc_in(size, count=nil, clear=true)
      self.__alloc_in(size, count, clear)
    end
    def self.alloc_out(size, count=nil, clear=true)
      self.__alloc_out(size, count, clear)
    end
    def self.alloc_inout(size, count=nil, clear=true)
      self.__alloc_inout(size, count, clear)
    end
  end
end