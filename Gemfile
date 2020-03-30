source 'https://rubygems.org'

group :development do
  gem 'rake', '~> 13.0'
  gem 'rake-compiler', '~> 1.0.3'
  gem 'rake-compiler-dock', '~> 1.0'
  gem 'rspec', '~> 3.0'
  gem 'rubygems-tasks', '~> 0.2.4', :require => 'rubygems/tasks'
  # irb is a dependency of rubygems-tasks 0.2.5. irb 1.1.1 is the last version to not depend on reline,
  # which sometimes causes 'bundle install' to fail on Ruby <= 2.4: https://github.com/rubygems/rubygems/issues/3463
  gem 'irb', '1.1.1'
  gem "rubysl", "~> 2.0", :platforms => 'rbx'
end

group :doc do
  gem 'kramdown'
  gem 'yard', '~> 0.9'
end
