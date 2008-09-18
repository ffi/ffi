module FFI
  class Buffer
    def self.__calc_size(type)
      if type.kind_of? Fixnum
        type
      elsif type.kind_of? Symbol
        FFI.type_size(type)
      else
        type.size
      end
    end
    def self.new(size, count=nil, clear=true)
      self.__alloc_inout(self.__calc_size(size), count, clear)
    end
    def self.alloc_in(size, count=nil, clear=true)
      self.__alloc_in(self.__calc_size(size), count, clear)
    end
    def self.alloc_out(size, count=nil, clear=true)
      self.__alloc_out(self.__calc_size(size), count, clear)
    end
    def self.alloc_inout(size, count=nil, clear=true)
      self.__alloc_inout(self.__calc_size(size), count, clear)
    end
  end
end