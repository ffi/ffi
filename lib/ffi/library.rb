module FFI::Library
  DEFAULT = FFI::DynamicLibrary.open(nil, FFI::DynamicLibrary::RTLD_LAZY)
 
  def ffi_lib(*names)
    ffi_libs = []
    names.each do |name|
      [ name, FFI.map_library_name(name) ].each do |libname|
        begin
          lib = FFI::DynamicLibrary.open(libname, FFI::DynamicLibrary::RTLD_LAZY | FFI::DynamicLibrary::RTLD_GLOBAL)
          if lib
            ffi_libs << lib
            break
          end
        rescue LoadError => ex
        end
      end
    end
    raise LoadError, "Could not open any of [#{names.join(", ")}]" if ffi_libs.empty?
    @ffi_libs = ffi_libs
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

    # Convert :foo to the native type
    arg_types.map! { |e| find_type(e) }
    has_callback = arg_types.any? {|t| t.kind_of?(FFI::CallbackInfo)}
    options = Hash.new
    options[:convention] = defined?(@ffi_convention) ? @ffi_convention : :default
    options[:type_map] = @ffi_typedefs if defined?(@ffi_typedefs)
    options[:enums] = @ffi_enums if defined?(@ffi_enums)

    # Try to locate the function in any of the libraries
    invokers = []
    libraries = defined?(@ffi_libs) ? @ffi_libs : [ DEFAULT ]
    libraries.each do |lib|
      begin
        invokers << FFI.create_invoker(lib, cname.to_s, arg_types, ret_type, find_type(ret_type), options)
      rescue LoadError => ex
      end if invokers.empty?
    end
    invoker = invokers.compact.shift
    raise FFI::NotFoundError.new(cname.to_s, libraries.map { |lib| lib.name }) unless invoker

    # Setup the parameter list for the module function as (a1, a2)
    arity = arg_types.length
    params = (1..arity).map {|i| "a#{i}" }.join(",")

    # Always use rest args for functions with callback parameters
    if has_callback || invoker.kind_of?(FFI::VariadicInvoker)
      params = "*args, &block"
    end
    call = arity <= 3 && !has_callback && !invoker.kind_of?(FFI::VariadicInvoker)? "call#{arity}" : "call"

    #
    # Attach the invoker to this module as 'mname'.
    #
    if !has_callback && !invoker.kind_of?(FFI::VariadicInvoker)
      invoker.attach(self, mname.to_s)
    else
      self.module_eval <<-code
        @@#{mname} = invoker
        def self.#{mname}(#{params})
          @@#{mname}.#{call}(#{params})
        end
        def #{mname}(#{params})
          @@#{mname}.#{call}(#{params})
        end
      code
    end
    invoker
  end
  def attach_variable(mname, a1, a2 = nil)
    cname, type = a2 ? [ a1, a2 ] : [ mname.to_s, a1 ]
    libraries = defined?(@ffi_libs) ? @ffi_libs : [ DEFAULT ]
    address = nil
    libraries.each do |lib|
      begin
        address = lib.find_variable(cname.to_s)
        break unless address.nil?
      rescue LoadError
      end
    end
    raise FFI::NotFoundError.new(cname, libraries) if address.nil?
    case ffi_type = find_type(type)
    when :pointer, FFI::NativeType::POINTER
      op = :pointer
    when :char, FFI::NativeType::INT8
      op = :int8
    when :uchar, FFI::NativeType::UINT8
      op = :uint8
    when :short, FFI::NativeType::INT16
      op = :int16
    when :ushort, FFI::NativeType::UINT16
      op = :uint16
    when :int, FFI::NativeType::INT32
      op = :int32
    when :uint, FFI::NativeType::UINT32
      op = :uint32
    when :long, FFI::NativeType::LONG
      op = :long
    when :ulong, FFI::NativeType::ULONG
      op = :ulong
    when :long_long, FFI::NativeType::INT64
      op = :int64
    when :ulong_long, FFI::NativeType::UINT64
      op = :uint64
    else
      if ffi_type.is_a?(FFI::CallbackInfo)
        op = :callback
      else
        raise FFI::TypeError, "Cannot access library variable of type #{type}"
      end
    end
    #
    # Attach to this module as mname/mname=
    #
    if op == :callback
      self.module_eval <<-code
        @@ffi_gvar_#{mname} = address
        @@ffi_gvar_#{mname}_cbinfo = ffi_type
        def self.#{mname}
          raise ArgError, "Cannot get callback fields"
        end
        def self.#{mname}=(value)
          @@ffi_gvar_#{mname}.put_callback(0, value, @@ffi_gvar_#{mname}_cbinfo)
        end
        code
    else
      self.module_eval <<-code
        @@ffi_gvar_#{mname} = address
        def self.#{mname}
          @@ffi_gvar_#{mname}.get_#{op.to_s}(0)
        end
        def self.#{mname}=(value)
          @@ffi_gvar_#{mname}.put_#{op.to_s}(0, value)
        end
        code
    end
    address
  end
  def callback(name, args, ret)
    @ffi_callbacks = Hash.new unless defined?(@ffi_callbacks)
    @ffi_callbacks[name] = FFI::CallbackInfo.new(find_type(ret), args.map { |e| find_type(e) })
  end
  def typedef(current, add, info=nil)
    @ffi_typedefs = Hash.new unless defined?(@ffi_typedefs)
    if current.kind_of?(FFI::Type)
      code = current
    else
      if current == :enum
        if add.kind_of?(Array)
          self.enum(add)
        else
          self.enum(info, add)
        end
      end
      code = @ffi_typedefs[current] || FFI.find_type(current)
    end

    @ffi_typedefs[add] = code
  end
  def enum(*args)
    values, tag = if args[0].kind_of?(Array)
      [ args[0], args[1] ]
    else
      [ args, nil ]
    end
    @ffi_enums = FFI::Enums.new unless defined?(@ffi_enums)
    @ffi_enums << (e = FFI::Enum.new(values, tag))
    e
  end
  def get_enum(query)
    return nil unless defined?(@ffi_enums)
    @ffi_enums.find(query)
  end
  def [](symbol)
    @ffi_enums.__map_symbol(symbol)
  end
  def find_type(name)
    code = if defined?(@ffi_typedefs) && @ffi_typedefs.has_key?(name)
      @ffi_typedefs[name]
    elsif defined?(@ffi_callbacks) && @ffi_callbacks.has_key?(name)
      @ffi_callbacks[name]
    elsif name.is_a?(Class) && name < FFI::Struct
      FFI::NativeType::POINTER
    elsif name.kind_of?(FFI::Type)
      name
    end
    if code.nil? || code.kind_of?(Symbol)
      FFI.find_type(name)
    else
      code
    end
  end
end