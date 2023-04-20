
module FFI
  class AbstractMemory
    # for TYPEXXX in FFI::TypeDefs.keys
    #  add respond_to? [:put_TYPEXXX, :get_TYPEXXX,
    #                   :write_TYPEXXX, :read_TYPEXXX,
    #                   :put_array_of_TYPEXXX, :get_array_of_TYPEXXX,
    #                   :write_array_of_TYPEXXX, :read_array_of_TYPEXXX]
    def method_missing method_name, *args, &block
      if /^(?<oper>(?:put|get|write|read)_(?:array_of_)?)(?<type_name>[[:word:]]+)$/ =~ method_name.to_s
        if builtintype = FFI::TypeDefs[type_name.intern] || FFI::TypeDefs[type_name]
          if /FFI\:\:Type\:\:Builtin\:(?<ori_type>[[:word:]]+)/ =~ builtintype.inspect
            ori_method1 = "#{oper}#{ori_type}"
            ori_method2 = "#{oper}#{ori_type.downcase}"
            if respond_to?(ori_method1, true)
              return send(ori_method1, *args, &block)
            elsif respond_to?(ori_method2, true)
              return send(ori_method2, *args, &block)
            end
          end
        end
      end

      super
    end
  end
end
