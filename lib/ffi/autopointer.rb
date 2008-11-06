module FFI
  class AutoPointer < Pointer
    def self.new(ptr, &block)
      klass = self
      free_lambda = block_given? ? lambda do block.call(ptr); end : lambda do klass.release(ptr); end
      ap = self.__alloc(ptr)
      ObjectSpace.define_finalizer(ap, free_lambda)
      ap
    end
    def self.release(ptr)
      
    end
  end
end