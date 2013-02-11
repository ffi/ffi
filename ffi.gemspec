Gem::Specification.new do |s|
  s.name = 'ffi'
  s.version = '0.6.4'
  s.author = 'Wayne Meissner'
  s.email = 'wmeissner@gmail.com'
  s.homepage = 'http://wiki.github.com/ffi/ffi'
  s.summary = 'Ruby FFI'
  s.description = 'Ruby FFI library'
  s.files = %w(ffi.gemspec History.txt LICENSE README.md Rakefile) + Dir.glob("{ext,gen,lib,spec,libtest}/**/*").reject { |f| f =~ /lib\/1\.[89]/}
  s.extensions << 'ext/ffi_c/extconf.rb'
  s.has_rdoc = false
  s.license = 'LGPL-3'
  s.require_paths << 'ext/ffi_c' << 'lib/ffi'
  s.required_ruby_version = '>= 1.8.7'
  s.add_development_dependency 'rake'
  s.add_development_dependency 'rake-compiler', '>=0.6.0'
  s.add_development_dependency 'rspec'
end
