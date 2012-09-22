#
# Copyright (C) 2008, 2009 Wayne Meissner
# Copyright (C) 2009 Luc Heinrich
# All rights reserved.
#
# This file is part of ruby-ffi.
#
# This code is free software: you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License version 3 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
# version 3 for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# version 3 along with this work.  If not, see <http://www.gnu.org/licenses/>.

module FFI
  class NotImplemented
    def initialize(cname, required_version, current_version)
      @cname = cname
      @required_version = required_version
      @current_version = current_version
    end

    #
    # Raise an error if the function is not available in the current library version
    #
    def call(*args)
      msg = "The function #{@cname} is only available in version #{@required_version} and higher. " +
            "You are running version #{@current_version}"
      raise(RuntimeError, msg)
    end

    #
    # Attach the invoker to module +mod+ as +mname+
    #
    def attach(mod, mname)
      invoker = self
      mod.module_eval <<-code
        @@#{mname} = invoker
        def self.#{mname}(*args)
          @@#{mname}.call(*args)
        end
        def #{mname}(*args)
          @@#{mname}.call(*args)
        end
      code
      invoker
    end
  end
end