require 'ffi/platform'
module FFI
  class StructLayout
    attr_reader :size, :align
    def initialize(fields, size, min_align = 1)
      @fields = fields
      @size = size
      @align = min_align
    end
    def members
      @fields.keys
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
        @@info = #{type}
        def self.size
          #{type.size} * 8
        end
        def self.align
          #{type.align}
        end
        def get(ptr)
          @@info.new(ptr + @off)
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
        @@info = #{type}
        @@num = #{num}
        def self.size
          #{type.size} * #{num}
        end
        def self.align
          #{type.align}
        end
        def get(ptr)
          @array ? @array : get_array_data(ptr)
        end
        private
        def get_array_data(ptr)
          @array = FFI::Struct::Array.new(ptr + @off, @@info, @@num)
        end
      end
      ArrayField_#{klass_name}_#{num}
      code
      self.module_eval(code)
    end
    class CallbackField < Field
      def self.size; Platform::ADDRESS_SIZE; end
      def self.align; Platform::ADDRESS_ALIGN; end
      def put(ptr, proc)
        ptr.put_callback(@off, proc, @info)
      end
      def get(ptr)
        raise ArgumentError, "Cannot get callback fields"
      end
    end
    def initialize
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
    def add_field(name, type, offset=nil)
      field_class, info = field_class_from(type)
      size = field_class.size / 8
      off = offset ? offset.to_i : align(@size, field_class.align)
      @fields[name] = field_class.new(off, info)
      @size = off + size
      @min_align = field_class.align if field_class.align > @min_align
    end
    def build
      StructLayout.new @fields, @size, @min_align
    end
    def align(offset, bits)
      bytes = bits / 8
      mask = bytes - 1;
      off = offset;
      ((off & mask) != 0) ? (off & ~mask) + bytes : off
    end
  end
  class Struct
    class Array
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
        @num * @type.size / 8
      end
      private
      def get_array_data(ptr)
        (0..@num - 1).inject([]) do |array, index| 
          array << @type.new(0).get(ptr + index * @type.size / 8)
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
    private
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
      builder = FFI::StructLayoutBuilder.new
      mod = enclosing_module
      spec[0].each do |name,type|
        builder.add_field(name, find_type(type, mod))
      end
      builder.build
    end
    def self.array_layout(spec)
      builder = FFI::StructLayoutBuilder.new
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
