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

  
  class Struct

    def self.by_value
      ::FFI::StructByValue.new(self)
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
        if type.kind_of?(Class) && type < Struct
          builder.add_struct(name, type)
        elsif type.kind_of?(Array)
          builder.add_array(name, find_type(type[0], mod), type[1])
        else
          builder.add_field(name, find_type(type, mod))
        end
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
        
        # If the next param is a Fixnum, it specifies the offset
        if spec[i].kind_of?(Fixnum)
          offset = spec[i]
          i += 1
        else
          offset = nil
        end
        if type.kind_of?(Class) && type < Struct
          builder.add_struct(name, type, offset)
        elsif type.kind_of?(::Array)
          builder.add_array(name, find_type(type[0], mod), type[1], offset)
        else
          builder.add_field(name, find_type(type, mod), offset)
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
