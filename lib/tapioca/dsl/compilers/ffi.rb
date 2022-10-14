# frozen_string_literal: true

begin
  require "ffi"
rescue LoadError
  return
end

module Tapioca
  module Dsl
    module Compilers
      # `Tapioca::Dsl::Compilers::FFI` generate types for functions and
      # variables bound by Ruby-FFI for dynamically-linked native libraries.
      # Ruby-FFI dynamically defines constants and methods at runtime. For
      # example, given a module:
      #
      #   module MyModule
      #     extend FFI::Library
      #     ffi_lib 'mylib'
      #
      #     attach_function :func, [:int, :pointer], :void
      #     attach_variable :var, :string
      #   end
      #
      # This will result in the following methods being defined:
      #
      #   func, var, var=
      #
      class FFI < Compiler
        def decorate
          root.create_path(constant) do |mod|
            constant.class_variables.each do |class_var|
              candidate = constant.class_variable_get(class_var)
              if candidate.is_a?(::FFI::Function)
                method_name = class_var.to_s.gsub(/\A@*/, "")
                if candidate.respond_to?(:type)
                  mod.create_method(
                    method_name,
                    parameters: params_for(candidate.type.param_types),
                    return_type: return_type_for(candidate.type.result_type),
                    class_method: true,
                  )
                else
                  mod.create_method(
                    method_name,
                    parameters: [create_rest_param("args", type: "T.untyped")],
                    return_type: "T.untyped",
                    class_method: true,
                  )
                end
              elsif candidate.is_a?(::FFI::VariadicInvoker)
                param_types =
                  params_for(candidate.instance_variable_get(:@fixed)) + [create_rest_param("rest", type: "T.untyped")]
                mod.create_method(
                  class_var.to_s.gsub(/\A@*/, ""),
                  parameters: param_types,
                  return_type: (
                    if candidate.respond_to?(:return_type)
                      return_type_for(candidate.return_type)
                    else
                      "T.untyped"
                    end
                  ),
                  class_method: true,
                )
              elsif candidate.is_a?(::FFI::Struct) && class_var =~ /\A@@ffi_gvar_(.+)\z/
                getter = T.must(::Regexp.last_match(1))
                return_type = return_type_for(candidate.layout.fields[0].type)
                mod.create_method(
                  getter,
                  parameters: [],
                  return_type: return_type,
                  class_method: true,
                )
                setter = "#{getter}="
                if constant.respond_to?(setter)
                  mod.create_method(
                    setter,
                    parameters: [
                      create_param("value", type: type_for(candidate.layout.fields[0].type)),
                    ],
                    return_type: return_type,
                    class_method: true,
                  )
                end
              end
            end
          end
        end

        private

        MAPPING = T.let({
          ::FFI::Type::BOOL => "T::Boolean",
          ::FFI::Type::BUFFER_IN => "::FFI::Pointer",
          ::FFI::Type::BUFFER_INOUT => "::FFI::Pointer",
          ::FFI::Type::BUFFER_OUT => "::FFI::Pointer",
          ::FFI::Type::CHAR => "Integer",
          ::FFI::Type::DOUBLE => "Float",
          ::FFI::Type::FLOAT => "Float",
          ::FFI::Type::FLOAT32 => "Float",
          ::FFI::Type::FLOAT64 => "Float",
          ::FFI::Type::INT => "Integer",
          ::FFI::Type::INT16 => "Integer",
          ::FFI::Type::INT32 => "Integer",
          ::FFI::Type::INT64 => "Integer",
          ::FFI::Type::INT8 => "Integer",
          ::FFI::Type::LONG => "Integer",
          ::FFI::Type::LONGDOUBLE => "Float",
          ::FFI::Type::LONG_LONG => "Integer",
          ::FFI::Type::POINTER => "::FFI::Pointer",
          ::FFI::Type::SCHAR => "Integer",
          ::FFI::Type::SHORT => "Integer",
          ::FFI::Type::SINT => "Integer",
          ::FFI::Type::SLONG => "Integer",
          ::FFI::Type::SLONG_LONG => "Integer",
          ::FFI::Type::SSHORT => "Integer",
          ::FFI::Type::STRING => "String",
          ::FFI::Type::UCHAR => "Integer",
          ::FFI::Type::UINT => "Integer",
          ::FFI::Type::UINT16 => "Integer",
          ::FFI::Type::UINT32 => "Integer",
          ::FFI::Type::UINT64 => "Integer",
          ::FFI::Type::UINT8 => "Integer",
          ::FFI::Type::ULONG => "Integer",
          ::FFI::Type::ULONG_LONG => "Integer",
          ::FFI::Type::USHORT => "Integer",
          ::FFI::Type::VOID => "void",
        }, T::Hash[::FFI::Type, String])

        def return_type_for(ffi_type)
          type_for(ffi_type, for_return: true)
        end

        def type_for(ffi_type, for_return: false)
          if ffi_type == ::FFI::Type::POINTER && !for_return
            # Special case, pointers as params can be one of several
            "T.nilable(T.any(::FFI::Pointer, String, Integer))"
          elsif ffi_type.is_a?(::FFI::Type::Mapped)
            mapped_type_for(ffi_type)
          elsif ffi_type.is_a?(::FFI::StructByValue)
            ffi_type.struct_class.class_name
          elsif ffi_type.is_a?(::FFI::Type::Array)
            "[#{Array.new(ffi_type.length, type_for(ffi_type.elem_type)).join(", ")}]"
          elsif ffi_type.is_a?(::FFI::Type::Function)
            params = ffi_type.param_types.map { |param_type| type_for(param_type).to_s }.join(", ")
            result = (
              if ffi_type.result_type == ::FFI::Type::VOID
                "void"
              else
                "returns(#{return_type_for(ffi_type.result_type)})"
              end
            )
            "T.proc.params(#{params}).#{result}"
          else
            MAPPING.fetch(ffi_type, "T::Untyped")
          end
        end

        def mapped_type_for(ffi_type)
          return "T::Untyped" unless ffi_type.respond_to?(:converter)

          converter = ffi_type.converter

          if converter.is_a?(::FFI::StructByReference)
            converter.struct_class.class_name
          elsif converter == ::FFI::StrPtrConverter
            "[String, ::FFI::Pointer]"
          else
            "T::Untyped"
          end
        end

        def params_for(ffi_types)
          ffi_types.map.with_index do |ffi_type, index|
            create_param("arg#{index}", type: type_for(ffi_type))
          end
        end

        class << self
          def gather_constants
            # Find all Modules that are:
            all_modules.select do |mod|
              # named (i.e. not anonymous)
              name_of(mod) &&
                # not singleton classes
                !mod.singleton_class? &&
                # extend ActiveSupport::Concern, and
                mod.singleton_class < ::FFI::Library
            end
          end
        end
      end
    end
  end
end
