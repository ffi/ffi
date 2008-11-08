module FFI
  class VariadicInvoker
    def VariadicInvoker.new(library, function, arg_types, ret_type, convention)
      invoker = self.__new(library, function, ret_type, convention)
      invoker.init(arg_types)
      invoker
    end
    def init(arg_types)
      @fixed = Array.new
      arg_types.each_with_index do |type, i|
        @fixed << type unless type == FFI::NativeType::VARARGS
      end
    end
    def call(*args, &block)
      param_types = Array.new(@fixed)
      param_values = Array.new
      @fixed.each_with_index do |t, i|
        param_values << args[i]
      end
      i = @fixed.length
      while i < args.length
        param_types << FFI.find_type(args[i])
        param_values << args[i + 1]
        i += 2
      end
      invoke(param_types, param_values, &block)
    end
  end
end