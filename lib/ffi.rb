#
# Version: CPL 1.0/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Common Public
# License Version 1.0 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.eclipse.org/legal/cpl-v10.html
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# Copyright (C) 2008 JRuby project
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the CPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the CPL, the GPL or the LGPL.
#
# Copyright (c) 2007, 2008 Evan Phoenix
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of the Evan Phoenix nor the names of its contributors 
#   may be used to endorse or promote products derived from this software 
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE 
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

require 'ffi.so'
require 'memorypointer'

module FFI
  #  Specialised error classes
  class TypeError < RuntimeError; end
  
  class SignatureError < RuntimeError; end
  
  class NotFoundError < RuntimeError
    def initialize(function, library)
      super("Function '#{function}' not found! (Looking in '#{library}' or this process)")
    end
  end
  
  TypeDefs = Hash.new
  def self.add_typedef(current, add)
    if current.kind_of? Integer
      code = current
    else
      code = TypeDefs[current]
      raise TypeError, "Unable to resolve type '#{current}'" unless code
    end

    TypeDefs[add] = code
  end
  def self.find_type(name)
    code = TypeDefs[name]
    raise TypeError, "Unable to resolve type '#{name}'" unless code
    return code
  end
  
  # Converts a char
  add_typedef(NativeType::INT8, :char)

  # Converts an unsigned char
  add_typedef(NativeType::UINT8, :uchar)
  
  # Converts an 8 bit int
  add_typedef(NativeType::INT8, :int8)

  # Converts an unsigned char
  add_typedef(NativeType::UINT8, :uint8)

  # Converts a short
  add_typedef(NativeType::INT16, :short)

  # Converts an unsigned short
  add_typedef(NativeType::UINT16, :ushort)
  
  # Converts a 16bit int
  add_typedef(NativeType::INT16, :int16)

  # Converts an unsigned 16 bit int
  add_typedef(NativeType::UINT16, :uint16)

  # Converts an int
  add_typedef(NativeType::INT32, :int)

  # Converts an unsigned int
  add_typedef(NativeType::UINT32, :uint)
  
  # Converts a 32 bit int
  add_typedef(NativeType::INT32, :int32)

  # Converts an unsigned 16 bit int
  add_typedef(NativeType::UINT32, :uint32)

  # Converts a long
  add_typedef(NativeType::LONG, :long)

  # Converts an unsigned long
  add_typedef(NativeType::ULONG, :ulong)
  
  # Converts a 64 bit int
  add_typedef(NativeType::INT64, :int64)

  # Converts an unsigned 64 bit int
  add_typedef(NativeType::UINT64, :uint64)

  # Converts a long long
  add_typedef(NativeType::INT64, :long_long)

  # Converts an unsigned long long
  add_typedef(NativeType::UINT64, :ulong_long)

  # Converts a float
  add_typedef(NativeType::FLOAT32, :float)

  # Converts a double
  add_typedef(NativeType::FLOAT64, :double)

  # Converts a pointer to opaque data
  add_typedef(NativeType::POINTER, :pointer)

  # For when a function has no return value
  add_typedef(NativeType::VOID, :void)

  # Converts NUL-terminated C strings
  add_typedef(NativeType::STRING, :string)
  
  # Use for a C struct with a char [] embedded inside.
  add_typedef(NativeType::CHAR_ARRAY, :char_array)
  
  TypeSizes = {
    1 => :char,
    2 => :short,
    4 => :int,
    8 => :long_long,
  }
end
module FFI::Platform
  LIBC = 'c'
  LIBPREFIX = 'lib'
  LIBSUFFIX = '.dylib'
end
module FFI
  def self.create_invoker(lib, name, args, ret, convention = :default)
    # Ugly hack to simulate the effect of dlopen(NULL, x) - not quite correct
    lib = FFI::Platform::LIBC unless lib
    
    # Mangle the library name to reflect the native library naming conventions
    lib = Platform::LIBPREFIX + lib unless lib =~ /^#{Platform::LIBPREFIX}/
    lib += Platform::LIBSUFFIX unless lib =~ /#{Platform::LIBPREFIX}$/
    
    # Current artificial limitation based on JRuby::FFI limit
    raise SignatureError, 'FFI functions may take max 32 arguments!' if args.size > 32

    invoker = FFI::Invoker.new(lib, name, args.map { |e| find_type(e) },
      find_type(ret), convention.to_s)
    raise NotFoundError.new(name, lib) unless invoker
    return invoker
  end
end

module FFI::Library
  # TODO: Rubinius does *names here and saves the array. Multiple libs?
  def ffi_lib(name)
    @ffi_lib = name
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
    lib = @ffi_lib
    convention = @ffi_convention ? @ffi_convention : :default
    invoker = FFI.create_invoker lib, cname.to_s, arg_types, ret_type, convention
    raise ArgumentError, "Unable to find function '#{cname}' to bind to #{self.name}.#{mname}" unless invoker
    self.class.send(:define_method, mname) do |*args|
      invoker.call(*args)
    end
    invoker
  end
end