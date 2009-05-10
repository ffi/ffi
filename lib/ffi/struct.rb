require 'ffi/platform'
module FFI

  class StructLayout
    attr_reader :size, :align

    def members
      @field_names
    end
    def offsets
      @fields.map { |name, field| [name, field.offset] }.sort { |a, b| a[1] <=> b[1] }
    end
    def offset_of(field_name)
      @fields[field_name].offset
    end
  end

  class StructLayoutBuilder
    class Field
      def size
        self.class.size
      end
      def align
        self.class.align
      end
      def offset
        @off
      end
      def self.size
        const_get(:SIZE)
      end
      def self.align
        const_get(:ALIGN)
      end
    end

    def self.struct_field_class_from(type)
      klass_name = type.name.split('::').last
      code = <<-code
      class StructField_#{klass_name} < Field
        @info = #{type}
          class << self
            attr_reader :info
            def size
              #{type.size}
            end
            def align
              #{type.align}
            end
          end
        def get(ptr)
          self.class.info.new(ptr + @off)
        end
      end
      StructField_#{klass_name}
      code
      self.module_eval(code)
    end

    def self.array_field_class_from(type, num)
      klass_name = type.name.split('::').last
      code = <<-code
      class ArrayField_#{klass_name}_#{num} < Field
        @info = #{type}
        @num = #{num}
          class << self
            attr_reader :info, :num
            def size
              #{type.size} * #{num}
            end
            def align
              #{type.align}
            end
          end
        def get(ptr)
          @array ? @array : get_array_data(ptr)
        end
        private
        def get_array_data(ptr)
          @array = FFI::Struct::Array.new(ptr + @off, self.class.info, self.class.num)
        end
      end
      ArrayField_#{klass_name}_#{num}
      code
      self.module_eval(code)
    end

    class CallbackField < Field
      def self.size
        FFI::Type::POINTER.size
      end

      def self.align
        FFI::Type::POINTER.alignment
      end

      def put(ptr, proc)
        ptr.put_callback(@off, proc, @info)
      end

      def get(ptr)
        raise ArgumentError, "Cannot get callback fields"
      end
    end

    def initialize
      @field_names = []
      @fields = {}
      @size = 0
      @min_align = 1
    end

    def native_field_class_from(type)
      case type
      when :char, NativeType::INT8
        Signed8
      when :uchar, NativeType::UINT8
        Unsigned8      
      when :short, NativeType::INT16
        Signed16
      when :ushort, NativeType::UINT16
        Unsigned16
      when :long, NativeType::LONG
        FFI::Platform::LONG_SIZE == 32 ? Signed32 : Signed64
      when :ulong, NativeType::ULONG
        FFI::Platform::LONG_SIZE == 32 ? Unsigned32 : Unsigned64
      when :int, NativeType::INT32
        Signed32
      when :uint, NativeType::UINT32
        Unsigned32
      when :long_long, NativeType::INT64
        Signed64
      when :ulong_long, NativeType::UINT64
        Unsigned64
      when :float, NativeType::FLOAT32
        FloatField
      when :double, NativeType::FLOAT64
        DoubleField
      when :pointer, NativeType::POINTER
        PointerField
      when :string, NativeType::STRING
        StringField
      end
    end

    def callback_field_class_from(type)
      return CallbackField, type if type.is_a?(FFI::CallbackInfo)
    end

    def struct_field_class_from(type)
      self.class.struct_field_class_from(type) if type.is_a?(Class) and type < FFI::Struct
    end

    def array_field_class_from(type)
      self.class.array_field_class_from(field_class_from(type[0]), type[1]) if type.is_a?(Array)
    end

    def field_class_from(type)
      field_class = native_field_class_from(type) ||
        callback_field_class_from(type) ||
        array_field_class_from(type) ||
        struct_field_class_from(type)
      field_class or raise ArgumentError, "Unknown type: #{type}"
    end

    def add_field(name, type, offset = nil)
      field_class, info = field_class_from(type)
      off = calc_alignment_of(field_class, offset)
      calc_current_size(off, field_class.size)
      @field_names << name
      @fields[name] = field_class.new(off, info)
      @min_align = field_class.align if field_class.align > @min_align
    end

    def build
      StructLayout.new(@field_names, @fields, align(@size, @min_align), @min_align)
    end

    def align(offset, align)
      align + ((offset - 1) & ~(align - 1))
    end

    private
    def calc_alignment_of(field_class, offset)
      offset ? offset.to_i : align(@size, field_class.align)
    end
    def calc_current_size(offset, size)
      @size = offset + size
    end
  end

  class Struct
    class Array
      include Enumerable

      def initialize(ptr, type, num)
        @pointer, @type, @num = ptr, type, num
      end

      def to_ptr
        @pointer 
      end

      def to_a
        get_array_data(@pointer)
      end

      def size
        @num * @type.size
      end

      def each(&blk)
        to_a.each(&blk)
      end

      private
      def get_array_data(ptr)
        (0..@num - 1).inject([]) do |array, index| 
          array << @type.new(0).get(ptr + index * @type.size)
        end
      end
    end

    def self.size
      @size
    end

    def self.members
      @layout.members
    end

    def self.align
      @layout.align
    end

    def self.offsets
      @layout.offsets
    end

    def self.offset_of(field_name)
      @layout.offset_of(field_name)
    end

    def size
      self.class.size
    end

    def align
      self.class.align
    end

    def members
      layout.members
    end

    def values
      layout.members.map { |m| self[m] }
    end
    def offsets
      self.class.offsets
    end

    def offset_of(field_name)
      self.class.offset_of(field_name)
    end

    def clear
      pointer.clear
      self
    end

    def to_ptr
      pointer
    end

    def self.in
      :buffer_in
    end

    def self.out
      :buffer_out
    end

    protected

    def self.callback(params, ret)
      mod = enclosing_module
      FFI::CallbackInfo.new(find_type(ret, mod), params.map { |e| find_type(e, mod) })
    end

    private

    def self.builder
      StructLayoutBuilder.new
    end

    def self.enclosing_module
      begin
        mod = self.name.split("::")[0..-2].inject(Object) { |obj, c| obj.const_get(c) }
        mod.respond_to?(:find_type) ? mod : nil
      rescue Exception => ex
        nil
      end
    end

    def self.is_a_struct?(type)
      type.is_a?(Class) and type < Struct
    end

    def self.find_type(type, mod = nil)
      return type if is_a_struct?(type) or type.is_a?(::Array)
      mod ? mod.find_type(type) : FFI.find_type(type)
    end

    def self.hash_layout(spec)
      raise "Ruby version not supported" if RUBY_VERSION =~ /1.8.*/
      builder = self.builder
      mod = enclosing_module
      spec[0].each do |name,type|
        builder.add_field(name, find_type(type, mod))
      end
      builder.build
    end

    def self.array_layout(spec)
      builder = self.builder
      mod = enclosing_module
      i = 0
      while i < spec.size
        name, type = spec[i, 2]
        i += 2
        code = find_type(type, mod) 
        # If the next param is a Fixnum, it specifies the offset
        if spec[i].kind_of?(Fixnum)
          offset = spec[i]
          i += 1
          builder.add_field(name, code, offset)
        else
          builder.add_field(name, code)
        end
      end
      builder.build
    end

    public
    def self.layout(*spec)
      return @layout if spec.size == 0
      cspec = spec[0].kind_of?(Hash) ? hash_layout(spec) : array_layout(spec)
      @layout = cspec unless self == FFI::Struct
      @size = cspec.size
      return cspec
    end
  end
end
