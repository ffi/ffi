require 'ffi/platform'
module FFI
  class StructLayout
    def initialize(fields, size)
      @fields = fields
      @size = size
    end
    def size
      @size
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
    end
    def add_field(name, type, offset=nil)
      info = nil
      field_class = case type
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
      when FFI::CallbackInfo
        info = type
        CallbackField
      when :string, NativeType::STRING
        StringField
      else
        raise ArgumentError, "Unknown type: #{type}"
      end
      
      size = field_class.size / 8
      off = offset ? offset.to_i : align(@size, field_class.align)
      @fields[name] = field_class.new(off, info)
      @size = off + size
    end
    def build
      StructLayout.new @fields, @size
    end
    def align(offset, bits)
      bytes = bits / 8
      mask = bytes - 1;
      off = offset;
      ((off & mask) != 0) ? (off & ~mask) + bytes : off
    end
  end
  class BaseStruct
    Buffer = FFI::Buffer

    def initialize(ptr = nil, *spec)
      self.layout = @cspec = self.class.layout(*spec)
      
      if ptr then
        self.pointer = ptr
      else
        self.pointer = MemoryPointer.new(@cspec.size)
      end
    end
    def self.alloc_inout(clear = true)
      self.new(Buffer.alloc_inout(@size, 1, clear))
    end
    def self.alloc_in(clear = true)
      self.new(Buffer.alloc_in(@size, 1, clear))
    end
    def self.alloc_out(clear = true)
      self.new(Buffer.alloc_out(@size, 1, clear))
    end
    def self.size
      @size
    end
    def self.members
      @layout.members
    end
    def size
      self.class.size
    end
    def members
      @cspec.members
    end
    def values
      @cspec.members.map { |m| self[m] }
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
  end
  class Struct < BaseStruct
    private
    def self.enclosing_module
      begin
        mod = self.name.split("::")[0..-2].inject(Object) { |obj, c| obj.const_get(c) }
        mod.respond_to?(:find_type) ? mod : nil
      rescue Exception => ex
        nil
      end
    end
    def self.find_type(type, mod = nil)
      return mod ? mod.find_type(type) : FFI.find_type(type)
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
        # If the next param is a Fixnu, it specifies the offset
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
