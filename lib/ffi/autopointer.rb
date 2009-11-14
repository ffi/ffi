module FFI
  class AutoPointer < Pointer

    # call-seq:
    #   AutoPointer.new(pointer, method)     => the passed Method will be invoked at GC time
    #   AutoPointer.new(pointer, proc)       => the passed Proc will be invoked at GC time (SEE WARNING BELOW!)
    #   AutoPointer.new(pointer) { |p| ... } => the passed block will be invoked at GC time (SEE WARNING BELOW!)
    #   AutoPointer.new(pointer)             => the pointer's release() class method will be invoked at GC time
    #
    # WARNING: passing a proc _may_ cause your pointer to never be GC'd, unless you're careful to avoid trapping a reference to the pointer in the proc. See the test specs for examples.
    # WARNING: passing a block will cause your pointer to never be GC'd. This is bad.
    #
    # Please note that the safest, and therefore preferred, calling
    # idiom is to pass a Method as the second parameter. Example usage:
    #
    #   class PointerHelper
    #     def self.release(pointer)
    #       ...
    #     end
    #   end
    #
    #   p = AutoPointer.new(other_pointer, PointerHelper.method(:release))
    #
    # The above code will cause PointerHelper#release to be invoked at GC time.
    #
    # The last calling idiom (only one parameter) is generally only
    # going to be useful if you subclass AutoPointer, and override
    # release(), which by default does nothing.
    #
    def initialize(ptr, proc=nil, &block)
      raise TypeError, "Invalid pointer" if ptr.nil? || !ptr.kind_of?(Pointer) \
        || ptr.kind_of?(MemoryPointer) || ptr.kind_of?(AutoPointer)

      @releaser = if proc
                    raise RuntimeError.new("proc must be callable") unless proc.respond_to?(:call)
                    CallableReleaser.new(ptr, proc)

                  else
                    raise RuntimeError.new("no release method defined") unless self.class.respond_to?(:release)
                    DefaultReleaser.new(ptr, self.class)
                  end

      self.parent = ptr
      ObjectSpace.define_finalizer(self, @releaser)
      self
    end

    def free
      @releaser.free
    end

    def autorelease=(autorelease)
      @releaser.autorelease=(autorelease)
    end

    class Releaser
      def initialize(ptr, proc)
        @ptr = ptr
        @proc = proc
        @autorelease = true
      end

      def free
        raise RuntimeError.new("pointer already freed") unless @ptr
        @autorelease = false
        @ptr = nil
        @proc = nil
      end
      
      def autorelease=(autorelease)
        raise RuntimeError.new("pointer already freed") unless @ptr
        @autorelease = autorelease
      end

    end

    class DefaultReleaser < Releaser
      def call(*args)
        @proc.release(@ptr) if @autorelease && @ptr
      end
    end

    class CallableReleaser < Releaser
      def call(*args)
        @proc.call(@ptr) if @autorelease && @ptr
      end
    end

  end

end
