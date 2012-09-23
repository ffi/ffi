#
# Copyright (C) 2008-2010 Wayne Meissner
# Copyright (C) 2008 Mike Dalessio
#
# All rights reserved.
#
# This file is part of ruby-ffi.
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

module FFI
  class AutoPointer < Pointer
    extend DataConverter

    # @overload initialize(pointer, method)
    #  @param [Pointer] pointer
    #  @param [Method] method
    #  @return [self]
    #  The passed Method will be invoked at GC time.
    # @overload initialize(pointer, proc)
    #  @param [Pointer] pointer
    #  @return [self]
    #  The passed Proc will be invoked at GC time (SEE WARNING BELOW!)
    #  @note WARNING: passing a proc _may_ cause your pointer to never be GC'd, unless you're 
    #   careful to avoid trapping a reference to the pointer in the proc. See the test 
    #   specs for examples.
    # @overload initialize(pointer) { |p| ... }
    #  @param [Pointer] pointer
    #  @yieldparam [Pointer] p +pointer+ passed to the block
    #  @return [self]
    #  The passed block will be invoked at GC time.
    #  @note WARNING: passing a block will cause your pointer to never be GC'd. This is bad.
    # @overload initialize(pointer)
    #  @param [Pointer] pointer
    #  @return [self]
    #  The pointer's release() class method will be invoked at GC time.
    #
    # @note The safest, and therefore preferred, calling
    #  idiom is to pass a Method as the second parameter. Example usage:
    #
    #   class PointerHelper
    #     def self.release(pointer)
    #       ...
    #     end
    #   end
    #
    #   p = AutoPointer.new(other_pointer, PointerHelper.method(:release))
    #
    #  The above code will cause PointerHelper#release to be invoked at GC time.
    #
    # @note
    #  The last calling idiom (only one parameter) is generally only
    #  going to be useful if you subclass {AutoPointer}, and override
    #  #release, which by default does nothing.
    def initialize(ptr, proc=nil, &block)
      super(ptr)
      raise TypeError, "Invalid pointer" if ptr.nil? || !ptr.kind_of?(Pointer) \
        || ptr.kind_of?(MemoryPointer) || ptr.kind_of?(AutoPointer)

      @releaser = if proc
                    raise RuntimeError.new("proc must be callable") unless proc.respond_to?(:call)
                    CallableReleaser.new(ptr, proc)

                  else
                    raise RuntimeError.new("no release method defined") unless self.class.respond_to?(:release)
                    DefaultReleaser.new(ptr, self.class)
                  end

      ObjectSpace.define_finalizer(self, @releaser)
      self
    end

    # @return [nil]
    # Free the pointer.
    def free
      @releaser.free
    end

    # @param [Boolean] autorelease
    # @return [Boolean] +autorelease+
    # Set +autorelease+ property. See {Pointer Autorelease section at Pointer}.
    def autorelease=(autorelease)
      @releaser.autorelease=(autorelease)
    end

    # @return [Boolean] +autorelease+
    # Get +autorelease+ property. See {Pointer Autorelease section at Pointer}.
    def autorelease?
      @releaser.autorelease
    end

    # @abstract Base class for {AutoPointer}'s releasers.
    #  
    #  All subclasses of Releaser should define a +#release(ptr)+ method.
    # A releaser is an object in charge of release an {AutoPointer}.
    class Releaser
      attr_accessor :autorelease

      # @param [Pointer] ptr
      # @param [#call] proc
      # @return [nil]
      # A new instance of Releaser.
      def initialize(ptr, proc)
        @ptr = ptr
        @proc = proc
        @autorelease = true
      end

      # @return [nil]
      # Free pointer.
      def free
        if @ptr
          release(@ptr)
          @autorelease = false
          @ptr = nil
          @proc = nil
        end
      end

      # @param args
      # Release pointer if +autorelease+ is set.
      def call(*args)
        release(@ptr) if @autorelease && @ptr
      end
      
    end

    # DefaultReleaser is a {Releaser} used when an {AutoPointer} is defined without Proc
    # or Method. In this case, the pointer to release must be of a class derived from 
    # AutoPointer with a +#release+ class method.
    class DefaultReleaser < Releaser
      # @param [Pointer] ptr
      # @return [nil]
      # Release +ptr+ by using his #release class method.
      def release(ptr)
        @proc.release(ptr)
      end
    end

    # CallableReleaser is a {Releaser} used when an {AutoPointer} is defined with a
    # Proc or a Method.
    class CallableReleaser < Releaser
      # @param [Pointer] ptr
      # @return [nil]
      # Release +ptr+ by using Proc or Method defined at +ptr+ {AutoPointer#initialize initialization}.
      def release(ptr)
        @proc.call(ptr)
      end
    end

    # Return native type of AutoPointer.
    #
    # Override {DataConverter#native_type}.
    # @return [Type::POINTER]
    # @raise {RuntimeError} if class does not implement a +#release+ method
    def self.native_type
      raise RuntimeError.new("no release method defined for #{self.inspect}") unless self.respond_to?(:release)
      Type::POINTER
    end

    # Create a new AutoPointer.
    #
    # Override {DataConverter#from_native}.
    # @overload self.from_native(ptr, ctx)
    # @param [Pointer] ptr 
    # @param ctx not used. Please set +nil+.
    # @return [AutoPointer]
    def self.from_native(val, ctx)
      self.new(val)
    end
  end

end
