if !defined?(RUBY_ENGINE) || RUBY_ENGINE == "ruby"
  begin
    if RUBY_VERSION =~ /1.8/
      require '1.8/ffi_c'
    elsif RUBY_VERSION =~ /1.9/
      require '1.9/ffi_c'
    else
      require 'ffi_c'
    end
  rescue Exception
    require 'ffi_c'
  end

  require 'ffi/ffi'

elsif defined?(RUBY_ENGINE) && RUBY_ENGINE == 'jruby'
  $LOAD_PATH.delete(File.dirname(__FILE__))
  $LOAD_PATH.delete(File.join(File.dirname(__FILE__), 'ffi'))
  $LOADED_FEATURES.delete(__FILE__) unless $LOADED_FEATURES.nil?
  require 'ffi.rb'
end
