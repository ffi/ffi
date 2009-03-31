require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

describe FFI::Struct, ' with inline callback functions' do
  it 'should be able to define inline callback field' do
    module CallbackMember
      extend FFI::Library
      ffi_lib TestLibrary::PATH
      class TestStruct < FFI::Struct
        layout \
          :add, callback([ :int, :int ], :int),
          :sub, callback([ :int, :int ], :int)
        end
      attach_function :struct_call_add_cb, [TestStruct, :int, :int], :int
      attach_function :struct_call_sub_cb, [TestStruct, :int, :int], :int
    end    
  end
end

