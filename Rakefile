require 'rubygems'
require 'rake/gempackagetask'
require 'rubygems/specification'
require "spec/rake/spectask"
require 'date'
require 'fileutils'
require 'rbconfig'

GEM = "ffi"
GEM_VERSION = "0.2.0"
AUTHOR = "Wayne Meissner"
EMAIL = "wmeissner@gmail.com"
HOMEPAGE = "http://kenai.com/projects/ruby-ffi"
SUMMARY = "A Ruby foreign function interface (compatible with Rubinius and JRuby FFI)"

spec = Gem::Specification.new do |s|
  s.name = GEM
  s.version = GEM_VERSION
  s.platform = Gem::Platform::RUBY
  s.has_rdoc = true
  s.extra_rdoc_files = ["README", "LICENSE"]
  s.summary = SUMMARY
  s.description = s.summary
  s.author = AUTHOR
  s.email = EMAIL
  s.homepage = HOMEPAGE
  s.rubyforge_project = 'ffi' 
  s.extensions = %w(ext/extconf.rb)
  
  s.require_path = 'lib'
  s.autorequire = GEM
  s.files = %w(LICENSE README Rakefile) + Dir.glob("{ext,lib,nbproject,samples,specs}/**/*")
end

desc "Run all specs"
Spec::Rake::SpecTask.new("specs") do |t|
  t.spec_opts = ["--format", "specdoc", "--colour"]
  t.spec_files = Dir["spec/**/*_spec.rb"].sort
end
desc "Run rubinius specs"
Spec::Rake::SpecTask.new("rbxspecs") do |t|
  t.spec_opts = ["--format", "specdoc", "--colour"]
  t.spec_files = Dir["spec/rbx/*_spec.rb"].sort
end

if RUBY_PLATFORM == "java"
  desc "Run specs"
  task :specs do
    sh %{#{Gem.ruby} -S spec #{Dir["specs/**/*_spec.rb"].join(" ")} -fs --color}
  end
  task :rbxspecs do
    sh %{#{Gem.ruby} -S spec #{Dir["specs/rbx/**/*_spec.rb"].join(" ")} -fs --color}
  end
else
  desc "Run specs"
  task :specs do
    ENV["MRI_FFI"] = "1"
    sh %{#{Gem.ruby} -S spec #{Dir["specs/**/*_spec.rb"].join(" ")} -fs --color}
  end
  task :rbxspecs do
    ENV["MRI_FFI"] = "1"
    sh %{#{Gem.ruby} -S spec #{Dir["specs/rbx/**/*_spec.rb"].join(" ")} -fs --color}
  end
end

Rake::GemPackageTask.new(spec) do |pkg|
  pkg.gem_spec = spec
end

desc "install the gem locally"
task :install => [:package] do
  sh %{sudo #{Gem.ruby} -S gem install pkg/#{GEM}-#{GEM_VERSION}}
end

desc "create a gemspec file"
task :make_spec do
  File.open("#{GEM}.gemspec", "w") do |file|
    file.puts spec.to_ruby
  end
end
file "Makefile" do
  sh %{#{Gem.ruby} ext/extconf.rb}
end
desc "Compile the native module"
task :compile => "Makefile" do
  sh %{make}
end
desc "Clean all built files"
task :clean do
  sh %{make clean} if File.exists?("Makefile")
  FileUtils.rm_rf("build")
  FileUtils.rm_rf("conftest.dSYM")
  FileUtils.rm_f(Dir["pkg/*.gem", "Makefile"])
end
LIBEXT = if Config::CONFIG['host_os'].downcase =~ /darwin/; "dylib"; else "so"; end
file "build/libtest.#{LIBEXT}" do
  sh %{make -f libtest/GNUmakefile}
end
desc "Test the extension"
task :test => [ :compile, "build/libtest.#{LIBEXT}", :specs ] do

end
