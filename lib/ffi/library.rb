#
# Copyright (C) 2008-2010 Wayne Meissner
# Copyright (C) 2008 Luc Heinrich <luc@honk-honk.com>
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

module FFI
  CURRENT_PROCESS = USE_THIS_PROCESS_AS_LIBRARY = Object.new

  # @param [#to_s] lib library name
  # @return [String] library name formatted for current platform
  # Transform a generic library name to a platform library name
  # @example
  #  # Linux
  #  FFI.map_library_name 'c'     # -> "libc.so.6"
  #  FFI.map_library_name 'jpeg'  # -> "libjpeg.so"
  #  # Windows
  #  FFI.map_library_name 'c'     # -> "msvcrt.dll"
  #  FFI.map_library_name 'jpeg'  # -> "jpeg.dll"
  def self.map_library_name(lib)
    # Mangle the library name to reflect the native library naming conventions
    lib = lib.to_s unless lib.kind_of?(String)
    lib = Library::LIBC if lib == 'c'

    if lib && File.basename(lib) == lib
      lib = Platform::LIBPREFIX + lib unless lib =~ /^#{Platform::LIBPREFIX}/
      r = Platform::IS_GNU ? "\\.so($|\\.[1234567890]+)" : "\\.#{Platform::LIBSUFFIX}$"
      lib += ".#{Platform::LIBSUFFIX}" unless lib =~ /#{r}/
    end

    lib
  end

  # Exception raised when a function is not found in libraries
  class NotFoundError < LoadError
    def initialize(function, *libraries)
      super("Function '#{function}' not found in [#{libraries[0].nil? ? 'current process' : libraries.join(", ")}]")
    end
  end

  # This module is the base to use native functions.
  #
  # A basic usage may be:
  #  require 'ffi'
  #  
  #  module Hello
  #    extend FFI::Library
  #    ffi_lib FFI::Library::LIBC
  #    attach_function 'puts', [ :string ], :int
  #  end
  #  
  #  Hello.puts("Hello, World")
  #
  # 
  module Library
    CURRENT_PROCESS = FFI::CURRENT_PROCESS
    LIBC = FFI::Platform::LIBC

    # @param mod extended object
    # @return [nil]
    # @raise {RuntimeError} if +mod+ is not a Module
    # Test if extended object is a Module. If not, raise RuntimeError.
    def self.extended(mod)
      raise RuntimeError.new("must only be extended by module") unless mod.kind_of?(Module)
    end

    
    # @param [Array] names names of libraries to load
    # @return [Array<DynamicLibrary>]
    # @raise {LoadError} if a library cannot be opened
    # Load native libraries.
    def ffi_lib(*names)
      lib_flags = defined?(@ffi_lib_flags) ? @ffi_lib_flags : FFI::DynamicLibrary::RTLD_LAZY | FFI::DynamicLibrary::RTLD_LOCAL
      ffi_libs = names.map do |name|

        if name == FFI::CURRENT_PROCESS
          FFI::DynamicLibrary.open(nil, FFI::DynamicLibrary::RTLD_LAZY | FFI::DynamicLibrary::RTLD_LOCAL)

        else
          libnames = (name.is_a?(::Array) ? name : [ name ]).map { |n| [ n, FFI.map_library_name(n) ].uniq }.flatten.compact
          lib = nil
          errors = {}

          libnames.each do |libname|
            begin
              lib = FFI::DynamicLibrary.open(libname, lib_flags)
              break if lib

            rescue Exception => ex
              ldscript = false
              if ex.message =~ /(([^ \t()])+\.so([^ \t:()])*):([ \t])*invalid ELF header/
                if File.read($1) =~ /GROUP *\( *([^ \)]+) *\)/
                  libname = $1
                  ldscript = true
                end
              end

              if ldscript
                retry
              else
                errors[libname] = ex
              end
            end
          end

          if lib.nil?
            raise LoadError.new(errors.values.join(".\n"))
          end

          # return the found lib
          lib
        end
      end

      @ffi_libs = ffi_libs
    end

    # Set the calling convention for {#attach_function} and {#callback}
    #
    # @see http://en.wikipedia.org/wiki/Stdcall#stdcall
    # @note +:stdcall+ is typically used for attaching Windows API functions
    #
    # @param [Symbol] convention one of +:default+, +:stdcall+
    # @return [Symbol] the new calling convention
    def ffi_convention(convention = nil)
      @ffi_convention ||= :default
      @ffi_convention = convention if convention
      @ffi_convention
    end

    # @see #ffi_lib
    # @return [Array<FFI::DynamicLibrary>] array of currently loaded FFI libraries
    # @raise [LoadError] if no libraries have been loaded (using {#ffi_lib})
    # Get FFI libraries loaded using {#ffi_lib}.
    def ffi_libraries
      raise LoadError.new("no library specified") if !defined?(@ffi_libs) || @ffi_libs.empty?
      @ffi_libs
    end

    # Flags used in {#ffi_lib}.
    #
    # This map allows you to supply symbols to {#ffi_lib_flags} instead of
    # the actual constants.
    FlagsMap = {
      :global => DynamicLibrary::RTLD_GLOBAL,
      :local => DynamicLibrary::RTLD_LOCAL,
      :lazy => DynamicLibrary::RTLD_LAZY,
      :now => DynamicLibrary::RTLD_NOW
    }

    # Sets library flags for {#ffi_lib}.
    #
    # @example
    #   ffi_lib_flags(:lazy, :local) # => 5
    #
    # @param [Symbol, …] flags (see {FlagsMap})
    # @return [Fixnum] the new value
    def ffi_lib_flags(*flags)
      @ffi_lib_flags = flags.inject(0) { |result, f| result | FlagsMap[f] }
    end


    ##
    # @overload attach_function(func, args, returns, options = {})
    #  @example attach function without an explicit name
    #    module Foo
    #      extend FFI::Library
    #      ffi_lib FFI::Library::LIBC
    #      attach_function :malloc, [:size_t], :pointer
    #    end
    #    # now callable via Foo.malloc
    # @overload attach_function(name, func, args, returns, options = {})
    #  @example attach function with an explicit name
    #    module Bar
    #      extend FFI::Library
    #      ffi_lib FFI::Library::LIBC
    #      attach_function :c_malloc, :malloc, [:size_t], :pointer
    #    end
    #    # now callable via Bar.c_malloc
    #
    # Attach C function +func+ to this module.
    #
    #
    # @param [#to_s] name name of ruby method to attach as
    # @param [#to_s] func name of C function to attach
    # @param [Array<Symbol>] args an array of types
    # @param [Symbol] returns type of return value
    # @option options [Boolean] :blocking (@blocking) set to true if the C function is a blocking call
    # @option options [Symbol] :convention (:default) calling convention (see {#ffi_convention})
    # @option options [FFI::Enums] :enums
    # @option options [Hash] :type_map
    #
    # @return [FFI::VariadicInvoker]
    #
    # @raise [FFI::NotFoundError] if +func+ cannot be found in the attached libraries (see {#ffi_lib})
    def attach_function(name, func, args, returns = nil, options = nil)
      mname, a2, a3, a4, a5 = name, func, args, returns, options
      cname, arg_types, ret_type, opts = (a4 && (a2.is_a?(String) || a2.is_a?(Symbol))) ? [ a2, a3, a4, a5 ] : [ mname.to_s, a2, a3, a4 ]

      # Convert :foo to the native type
      arg_types.map! { |e| find_type(e) }
      options = {
        :convention => ffi_convention,
        :type_map => defined?(@ffi_typedefs) ? @ffi_typedefs : nil,
        :blocking => defined?(@blocking) && @blocking,
        :enums => defined?(@ffi_enums) ? @ffi_enums : nil,
      }

      @blocking = false
      options.merge!(opts) if opts && opts.is_a?(Hash)

      # Try to locate the function in any of the libraries
      invokers = []
      ffi_libraries.each do |lib|
        if invokers.empty?
          begin
            function = nil
            function_names(cname, arg_types).find do |fname|
              function = lib.find_function(fname)
            end
            raise LoadError unless function

            invokers << if arg_types.length > 0 && arg_types[arg_types.length - 1] == FFI::NativeType::VARARGS
              VariadicInvoker.new(function, arg_types, find_type(ret_type), options)

            else
              Function.new(find_type(ret_type), arg_types, function, options)
            end

          rescue LoadError
          end
        end
      end
      invoker = invokers.compact.shift
      raise FFI::NotFoundError.new(cname.to_s, ffi_libraries.map { |lib| lib.name }) unless invoker

      invoker.attach(self, mname.to_s)
      invoker
    end

    # @param [#to_s] name function name
    # @param [Array] arg_types function's argument types
    # @return [Array<String>]
    # This function returns a list of possible names to lookup.
    # @note Function names on windows may be decorated if they are using stdcall. See
    #   * http://en.wikipedia.org/wiki/Name_mangling#C_name_decoration_in_Microsoft_Windows
    #   * http://msdn.microsoft.com/en-us/library/zxk0tw93%28v=VS.100%29.aspx
    #   * http://en.wikibooks.org/wiki/X86_Disassembly/Calling_Conventions#STDCALL
    #   Note that decorated names can be overridden via def files.  Also note that the
    #   windows api, although using, doesn't have decorated names.
    def function_names(name, arg_types)
      result = [name.to_s]
      if ffi_convention == :stdcall
        # Get the size of each parameter
        size = arg_types.inject(0) do |mem, arg|
          mem + arg.size
        end

        # Next, the size must be a multiple of 4
        size += (4 - size) % 4

        result << "_#{name.to_s}@#{size}" # win32
        result << "#{name.to_s}@#{size}" # win64
      end
      result
    end

    # @overload attach_variable(mname, cname, type)
    #  @example
    #   module Bar
    #     extend FFI::Library
    #     ffi_lib 'my_lib'
    #     attach_variable :c_myvar, :myvar, :long
    #   end
    #   # now callable via Bar.c_myvar
    # @overload attach_variable(cname, type)
    #  @example
    #   module Bar
    #     extend FFI::Library
    #     ffi_lib 'my_lib'
    #     attach_variable :myvar, :long
    #   end
    #   # now callable via Bar.myvar
    # @param [#to_s] mname name of ruby method to attach as
    # @param [#to_s] cname name of C variable to attach
    # @param [DataConverter, Struct, Symbol, Type] type C varaible's type
    # @return [DynamicLibrary::Symbol]
    # @raise {FFI::NotFoundError} if +cname+ cannot be found in libraries
    #
    # Attach C variable +cname+ to this module.
    def attach_variable(mname, a1, a2 = nil)
      cname, type = a2 ? [ a1, a2 ] : [ mname.to_s, a1 ]
      address = nil
      ffi_libraries.each do |lib|
        begin
          address = lib.find_variable(cname.to_s)
          break unless address.nil?
        rescue LoadError
        end
      end

      raise FFI::NotFoundError.new(cname, ffi_libraries) if address.nil? || address.null?
      if type.is_a?(Class) && type < FFI::Struct
        # If it is a global struct, just attach directly to the pointer
        s = type.new(address)
        self.module_eval <<-code, __FILE__, __LINE__
          @@ffi_gvar_#{mname} = s
          def self.#{mname}
            @@ffi_gvar_#{mname}
          end
code

      else
        sc = Class.new(FFI::Struct)
        sc.layout :gvar, find_type(type)
        s = sc.new(address)
        #
        # Attach to this module as mname/mname=
        #
        self.module_eval <<-code, __FILE__, __LINE__
          @@ffi_gvar_#{mname} = s
          def self.#{mname}
            @@ffi_gvar_#{mname}[:gvar]
          end
          def self.#{mname}=(value)
            @@ffi_gvar_#{mname}[:gvar] = value
          end
        code

      end

      address
    end


    # @overload callback(name, params, ret)
    # @overload callback(params, ret)
    # @param name callback name to add to type map
    # @param [Array] params array of parameters' types
    # @param [DataConverter, Struct, Symbol, Type] ret callback return type
    # @return [FFI::CallbackInfo]
    def callback(*args)
      raise ArgumentError, "wrong number of arguments" if args.length < 2 || args.length > 3
      name, params, ret = if args.length == 3
        args
      else
        [ nil, args[0], args[1] ]
      end

      options = Hash.new
      options[:convention] = ffi_convention
      options[:enums] = @ffi_enums if defined?(@ffi_enums)
      cb = FFI::CallbackInfo.new(find_type(ret), params.map { |e| find_type(e) }, options)

      # Add to the symbol -> type map (unless there was no name)
      unless name.nil?
        typedef cb, name
      end

      cb
    end

    # @param [DataConverter, Symbol, Type] old
    # @param  add
    # @param [] info
    # @return [FFI::Enum, FFI::Type]
    # Register or get an already registered type definition.
    #
    # To register a new type definition, +old+ should be a {FFI::Type}. +add+
    # is in this case the type definition.
    #
    # If +old+ is a {DataConverter}, a {Type::Mapped} is returned.
    #
    # If +old+ is +:enum+
    # * and +add+ is an +Array+, a call to {#enum} is made with +add+ as single parameter;
    # * in others cases, +info+ is used to create a named enum.
    #
    # If +old+ is a key for type map, #typedef get +old+ type definition.
    def typedef(old, add, info=nil)
      @ffi_typedefs = Hash.new unless defined?(@ffi_typedefs)

      @ffi_typedefs[add] = if old.kind_of?(FFI::Type)
        old

      elsif @ffi_typedefs.has_key?(old)
        @ffi_typedefs[old]

      elsif old.is_a?(DataConverter)
        FFI::Type::Mapped.new(old)

      elsif old == :enum
        if add.kind_of?(Array)
          self.enum(add)
        else
          self.enum(info, add)
        end

      else
        FFI.find_type(old)
      end
    end

    # @overload enum(name, values)
    #  Create a named enum.
    #  @example
    #   enum :foo, [:zero, :one, :two]  # named enum
    #  @param [Symbol] name name for new enum
    #  @param [Array] values values for enum
    # @overload enum(*args)
    #  Create an unnamed enum.
    #  @example
    #   enum :zero, :one, :two  # unnamed enum
    #  @param args values for enum
    # @overload enum(values)
    #  Create an unnamed enum.
    #  @example
    #   enum [:zero, :one, :two]  # unnamed enum, equivalent to above example
    #  @param [Array] values values for enum
    # @return [FFI::Enum]
    # Create a new {FFI::Enum}.
    def enum(*args)
      name, values = if args[0].kind_of?(Symbol) && args[1].kind_of?(Array)
        [ args[0], args[1] ]
      elsif args[0].kind_of?(Array)
        [ nil, args[0] ]
      else
        [ nil, args ]
      end
      @ffi_enums = FFI::Enums.new unless defined?(@ffi_enums)
      @ffi_enums << (e = FFI::Enum.new(values, name))

      # If called as enum :foo, [ :zero, :one, :two ], add a typedef alias
      typedef(e, name) if name
      e
    end

    # @param name
    # @return [FFI::Enum]
    # Find an enum by name.
    def enum_type(name)
      @ffi_enums.find(name) if defined?(@ffi_enums)
    end

    # @param symbol
    # @return [FFI::Enum]
    # Find an enum by a symbol it contains.
    def enum_value(symbol)
      @ffi_enums.__map_symbol(symbol)
    end

    # @param [DataConverter, Type, Struct, Symbol] t type to find
    # @return [Type]
    # Find a type definition.
    def find_type(t)
      if t.kind_of?(Type)
        t

      elsif defined?(@ffi_typedefs) && @ffi_typedefs.has_key?(t)
        @ffi_typedefs[t]

      elsif t.is_a?(Class) && t < Struct
        Type::POINTER

      elsif t.is_a?(DataConverter)
        # Add a typedef so next time the converter is used, it hits the cache
        typedef Type::Mapped.new(t), t

      end || FFI.find_type(t)
    end
  end
end
