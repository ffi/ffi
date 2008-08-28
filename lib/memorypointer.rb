module FFI
  class MemoryPointer
    def self.new(size, count = nil, clear = true)
      self.__allocate(size * (count ? count : 1), clear)
    end
  end
end