#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#

require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

describe "A function attached with blocking: :offload" do
  module LibTestOffload
    extend FFI::Library
    ffi_lib TestLibrary::PATH

    callback :cbVrV, [], :void
    attach_function :testAddOffload, :testAdd, [:int, :int], :int, blocking: :offload
    attach_function :testAddBlocking, :testAdd, [:int, :int], :int, blocking: true
    attach_function :testClosureVrVOffload, :testClosureVrV, [:cbVrV], :void, blocking: :offload
    attach_function :setLastErrorOffload, :setLastError, [:int], :void, blocking: :offload
    attach_function :packVarargsOffload, :pack_varargs, [:buffer_out, :string, :varargs], :void, blocking: :offload

    @blocking = :offload
    attach_function :testAddModuleOffload, :testAdd, [:int, :int], :int
  end

  # Runs each blocking operation on a new thread, recording that it was asked to.
  class OffloadRecordingScheduler
    attr_reader :operations

    def initialize
      @operations = 0
    end

    def blocking_operation_wait(work)
      @operations += 1
      Fiber.blocking { Thread.new { work.call }.value }
    end

    def fiber(&block)
      Fiber.new(blocking: false, &block).tap(&:resume)
    end

    def kernel_sleep(*args)
      Fiber.blocking { sleep(*args) }
    end

    def block(*); raise NotImplementedError; end
    def unblock(*); end
    def io_wait(*); raise NotImplementedError; end
    def fiber_interrupt(*); end
    def close; end
  end

  def in_scheduled_fiber(scheduler)
    Thread.new do
      Fiber.set_scheduler(scheduler)
      result = nil
      Fiber.schedule { result = yield }
      result
    end.value
  end

  it "behaves like blocking: true without a fiber scheduler" do
    expect(LibTestOffload.testAddOffload(1, 2)).to eq(3)
  end

  context "inside a non-blocking fiber" do
    before do
      skip "Fiber::Scheduler#blocking_operation_wait requires CRuby 3.4+" unless RUBY_ENGINE == "ruby" && RUBY_VERSION >= "3.4"
    end

    let(:scheduler) { OffloadRecordingScheduler.new }

    it "is handed to the scheduler's blocking_operation_wait" do
      expect(in_scheduled_fiber(scheduler) { LibTestOffload.testAddOffload(1, 2) }).to eq(3)
      expect(scheduler.operations).to eq(1)
    end

    it "is not handed to the scheduler when attached with blocking: true" do
      expect(in_scheduled_fiber(scheduler) { LibTestOffload.testAddBlocking(1, 2) }).to eq(3)
      expect(scheduler.operations).to eq(0)
    end

    it "runs callbacks invoked from the offloaded call" do
      calls = in_scheduled_fiber(scheduler) do
        count = 0
        LibTestOffload.testClosureVrVOffload { count += 1 }
        count
      end
      expect(calls).to eq(1)
      expect(scheduler.operations).to eq(1)
    end

    it "raises exceptions from callbacks invoked from the offloaded call" do
      error = in_scheduled_fiber(scheduler) do
        LibTestOffload.testClosureVrVOffload { raise ArgumentError, "from callback" }
        nil
      rescue ArgumentError => e
        e
      end
      expect(error&.message).to eq("from callback")
    end

    it "hands a variadic function to the scheduler" do
      buf = FFI::Buffer.new(:long_long, 2)
      in_scheduled_fiber(scheduler) { LibTestOffload.packVarargsOffload(buf, "ii", :int, 1, :int, 2) }
      expect(buf.get_int64(0)).to eq(1)
      expect(buf.get_int64(8)).to eq(2)
      expect(scheduler.operations).to eq(1)
    end

    it "is handed to the scheduler when the library sets @blocking = :offload" do
      expect(in_scheduled_fiber(scheduler) { LibTestOffload.testAddModuleOffload(1, 2) }).to eq(3)
      expect(scheduler.operations).to eq(1)
    end

    it "keeps errno set by the offloaded call" do
      errno = in_scheduled_fiber(scheduler) do
        LibTestOffload.setLastErrorOffload(0x12345678)
        FFI.errno
      end
      expect(errno).to eq(0x12345678)
    end
  end
end
