#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#

require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

# A library that owns background threads has to stay mapped for as long as they run.
# Unloading it leaves those threads executing code that is no longer mapped, and the
# first one to wake takes the process down.
#
# POSIX only: Windows unloads through a separate dl_open implementation, and the fixture
# only starts threads with pthreads.
describe "Library unloading", if: RUBY_ENGINE == "ruby" && !FFI::Platform.windows? && FFI::DynamicLibrary::RTLD_NODELETE != 0 do
  def run_child(script_name, *arguments)
    script = File.join(File.dirname(__FILE__), "fixtures", script_name)
    pid = spawn(RbConfig.ruby, "-Ilib", script, *arguments, out: File::NULL, err: File::NULL)
    Timeout.timeout(60) { Process.wait2(pid) }.last
  rescue Timeout::Error
    Process.kill(9, pid)
    raise
  end

  def run_detached_thread_child(mode)
    run_child("detached_thread_exit.rb", mode, TestLibrary::PATH)
  end

  # 42 means every thread started, 43 that at least one failed to start. A signal means
  # the library was unloaded while the threads were running.
  def expect_child_to_survive(status)
    expect(status.termsig).to be_nil, "child died from signal #{status.termsig}"
    expect(status.exitstatus).to eq(42)
  end

  it "keeps a default ffi_lib library mapped after its module is collected" do
    expect_child_to_survive(run_detached_thread_child("collect"))
  end

  it "keeps a default ffi_lib library mapped through interpreter shutdown" do
    expect_child_to_survive(run_detached_thread_child("shutdown"))
  end

  it "unloads a directly opened library without RTLD_NODELETE",
      if: FFI::DynamicLibrary.const_defined?(:RTLD_NOLOAD) && FFI::DynamicLibrary::RTLD_NOLOAD != 0 do
    status = run_child("dynamic_library_unload.rb", TestLibrary::PATH)
    expect(status.termsig).to be_nil, "child died from signal #{status.termsig}"
    expect(status.exitstatus).to eq(42), "child found library still loaded"
  end
end
