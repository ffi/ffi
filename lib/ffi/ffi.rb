#
# Copyright (C) 2008 JRuby project
# Copyright (c) 2007, 2008 Evan Phoenix
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of the Evan Phoenix nor the names of its contributors 
#   may be used to endorse or promote products derived from this software 
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE 
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

module FFI
  #  Specialised error classes
  class NativeError < LoadError; end
  
  class TypeError < NativeError; end
  
  class SignatureError < NativeError; end
  
  class NotFoundError < NativeError
    def initialize(function, *libraries)
      super("Function '#{function}' not found in [#{libraries[0].nil? ? 'current process' : libraries.join(", ")}]")
    end
  end
end

require 'ffi/platform'
require 'ffi/types'
require 'ffi/errno'
require 'ffi/memorypointer'
require 'ffi/buffer'
require 'ffi/struct'
require 'ffi/callback'
require 'ffi/io'
require 'ffi/autopointer'
require 'ffi/variadic'

module FFI
  
  def self.map_library_name(lib)
    # Mangle the library name to reflect the native library naming conventions
    lib = Platform::LIBC if Platform::IS_LINUX && lib == 'c'
    if lib && File.basename(lib) == lib
      ext = ".#{Platform::LIBSUFFIX}"
      lib = Platform::LIBPREFIX + lib unless lib =~ /^#{Platform::LIBPREFIX}/
      lib += ext unless lib =~ /#{ext}/
    end
    lib
  end
  def self.create_invoker(lib, name, args, ret, options = { :convention => :default })
    lib = FFI.map_library_name(lib)
    # Current artificial limitation based on JRuby::FFI limit
    raise SignatureError, 'FFI functions may take max 32 arguments!' if args.size > 32
    library = NativeLibrary.open(lib, 0)
    function = library.find_symbol(name)
    raise NotFoundError.new(name, lib) unless function
    args = args.map {|e| find_type(e) }
    if args.length > 0 && args[args.length - 1] == FFI::NativeType::VARARGS
      invoker = FFI::VariadicInvoker.new(library, function, args, find_type(ret), options)
    else
      invoker = FFI::Invoker.new(library, function, args, find_type(ret), options[:convention].to_s)
    end
    raise NotFoundError.new(name, lib) unless invoker
    return invoker
  end
end

module FFI::Library
  # TODO: Rubinius does *names here and saves the array. Multiple libs?
  def ffi_lib(*names)
    @ffi_lib = names
  end
  def ffi_convention(convention)
    @ffi_convention = convention
  end
  ##
  # Attach C function +name+ to this module.
  #
  # If you want to provide an alternate name for the module function, supply
  # it after the +name+, otherwise the C function name will be used.#
  #
  # After the +name+, the C function argument types are provided as an Array.
  #
  # The C function return type is provided last.

  def attach_function(mname, a3, a4, a5=nil)
    cname, arg_types, ret_type = a5 ? [ a3, a4, a5 ] : [ mname.to_s, a3, a4 ]
    libraries = defined?(@ffi_lib) ? @ffi_lib : [ nil ]
    convention = defined?(@ffi_convention) ? @ffi_convention : :default
    
    # Convert :foo to the native type
    callback_count = 0
    arg_types.map! { |e|
      begin
        find_type(e)
      rescue FFI::TypeError => ex
        if defined?(@ffi_callbacks) && @ffi_callbacks.has_key?(e)
          callback_count += 1
          @ffi_callbacks[e]
        elsif e.is_a?(Class) && e < FFI::Struct
          FFI::NativeType::POINTER
        else
          raise ex
        end
      end
    }
    options = Hash.new
    options[:convention] = convention
    options[:type_map] = @ffi_typedefs if defined?(@ffi_typedefs)
    # Try to locate the function in any of the libraries
    invoker = libraries.collect do |lib|
      begin
        FFI.create_invoker lib, cname.to_s, arg_types, find_type(ret_type), options
      rescue FFI::NotFoundError => ex
        nil
      end
    end.compact.shift
    raise FFI::NotFoundError.new(cname.to_s, *libraries) unless invoker

    # Setup the parameter list for the module function as (a1, a2)
    arity = arg_types.length
    params = (1..arity).map {|i| "a#{i}" }.join(",")
    
    # Always use rest args for functions with callback parameters
    if callback_count > 0 || invoker.kind_of?(FFI::VariadicInvoker)
      params = "*args, &block"
    end
    call = arity <= 3 && callback_count < 1 && !invoker.kind_of?(FFI::VariadicInvoker)? "call#{arity}" : "call"

    #
    # Attach the invoker to this module as 'mname'.
    #
    self.module_eval <<-code
      @@#{mname} = invoker
      def self.#{mname}(#{params})
        @@#{mname}.#{call}(#{params})
      end
      def #{mname}(#{params})
        @@#{mname}.#{call}(#{params})
      end
    code
    invoker
  end
  def callback(name, args, ret)
    @ffi_callbacks = Hash.new unless defined?(@ffi_callbacks)
    @ffi_callbacks[name] = FFI::CallbackInfo.new(find_type(ret), args.map { |e| find_type(e) })
  end
  def typedef(current, add)
    @ffi_typedefs = Hash.new unless defined?(@ffi_typedefs)
    if current.kind_of? Integer
      code = current
    else
      code = @ffi_typedefs[current] || FFI.find_type(current)
    end

    @ffi_typedefs[add] = code
  end
  def find_type(name)
    code = if defined?(@ffi_typedefs)
      @ffi_typedefs[name]
    end
    code = name if !code && name.kind_of?(FFI::CallbackInfo)
    if code.nil? || code.kind_of?(Symbol)
      FFI.find_type(name)
    else
      code
    end
  end
end

