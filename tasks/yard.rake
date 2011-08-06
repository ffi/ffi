begin
  require 'yard'

  namespace :doc do
    YARD::Rake::YardocTask.new do |yard|
    end
  end
rescue LoadError
  warn "[warn] YARD unavailable"
end

