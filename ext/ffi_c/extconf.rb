#!/usr/bin/env ruby

if !defined?(RUBY_ENGINE) || RUBY_ENGINE == "ruby"
  require 'mkmf'
  require 'rbconfig'
  dir_config("ffi_c")
  
  if pkg_config("libffi") ||
     have_header("ffi.h") ||
     find_header("ffi.h", "/usr/local/include")
  else
    puts <<-EOL
      Cannot find the include file <ffi.h>. Please specify its location by using one
      of the following options:

      --with-ffi_c-dir=/path/to/ffi
      --with-ffi_c-include=/path/to/ffi/include
    EOL
    exit 1
  end

  if RbConfig::CONFIG['host_os'].match(/mswin/i)
    find_header("stdbool.h", "./msvc")
    find_header("sys/param.h", "./msvc")
  end

  # We need at least ffi_call and ffi_prep_closure
  libffi_ok = have_library("ffi", "ffi_call", [ "ffi.h" ]) ||
              have_library("libffi", "ffi_call", [ "ffi.h" ])
  libffi_ok &&= have_func("ffi_prep_closure")

  unless libffi_ok
    puts <<-EOL
      Cannot find the library libffi. Please specify its location by using one
      of the following options:

      --with-ffi_c-dir=/path/to/ffi
      --with-ffi_c-lib=/path/to/ffi/lib
    EOL
    exit 1
  end

  # Check if the raw api is available.
  $defs << "-DHAVE_RAW_API" if have_func("ffi_raw_call") && have_func("ffi_prep_raw_closure")

  have_func('rb_thread_blocking_region')
  have_func('ruby_thread_has_gvl_p') unless RUBY_VERSION >= "1.9.3"
  have_func('ruby_native_thread_p')
  have_func('rb_thread_call_with_gvl')
  
  $defs << "-DHAVE_EXTCONF_H" if $defs.empty? # needed so create_header works
  $defs << "-DUSE_INTERNAL_LIBFFI" unless libffi_ok
  $defs << "-DRUBY_1_9" if RUBY_VERSION >= "1.9.0"

  create_header
  
  $CFLAGS << " -mwin32 " if RbConfig::CONFIG['host_os'] =~ /cygwin/
  #$CFLAGS << " -Werror -Wunused -Wformat -Wimplicit -Wreturn-type "
  if (ENV['CC'] || RbConfig::MAKEFILE_CONFIG['CC'])  =~ /gcc/
    $CFLAGS << " -Wno-declaration-after-statement "
  end
  
  create_makefile("ffi_c")
  unless libffi_ok
    File.open("Makefile", "a") do |mf|
      mf.puts "LIBFFI_HOST=--host=#{RbConfig::CONFIG['host_alias']}" if RbConfig::CONFIG.has_key?("host_alias")
      if RbConfig::CONFIG['host_os'].downcase =~ /darwin/
        mf.puts "include ${srcdir}/libffi.darwin.mk"
      elsif RbConfig::CONFIG['host_os'].downcase =~ /bsd/
        mf.puts '.include "${srcdir}/libffi.bsd.mk"'
      elsif RbConfig::CONFIG['host_os'] !~ /mswin32|mingw32/
        mf.puts "include ${srcdir}/libffi.mk"
      end
    end
  end
  
else
  File.open("Makefile", "w") do |mf|
    mf.puts "# Dummy makefile for non-mri rubies"
    mf.puts "all install::\n"
  end
end