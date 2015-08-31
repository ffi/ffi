#
# This file is part of ruby-ffi.
# For licensing, see LICENSE.SPECS
#

# test me via:
#   rspec -O spec/spec.opts

require File.expand_path(File.join(File.dirname(__FILE__), "spec_helper"))

# Define some sample structs for test purposes
class Simple < FFI::Struct
    packed
    layout  :one, :uint8,
            :two, :uint8,
            :three, :uint8
end
class Inner < FFI::Struct
    packed
    layout  :one, :uint8,
            :two, :uint32,
            :three, :uint16
end
class Outer < FFI::Struct
    packed
    layout  :header, :uint8,
            :nested, Inner,
            :footer, :uint16
end

describe FFI::Struct, :new do

  describe "#to_bytes" do
    it "returns a String of the proper length" do
      x = Inner.new
      bytes = x.to_bytes
      expect(bytes).to be_a(String)
      expect(bytes.bytesize).to eql(x.size)
    end

    it "dumps fields in the correct order" do
      x = Simple.new
      x[:one] = 1
      x[:two] = 2
      x[:three] = 3
      expect(x.to_bytes).to eql("\x01\x02\x03")
    end
  end


  describe "#init_from_bytes" do
    it "can import data from a bytestring" do
      expect {
        @x = Inner.new
        @x.init_from_bytes("\x01\x02\x00\x00\x00\x03\x00")
      }.not_to raise_error
      expect(@x[:one]).to eql 1
      expect(@x[:two]).to eql 2
      expect(@x[:three]).to eql 3
    end
    it "will not import data from a bytestring with the wrong length" do
      @x = Inner.new
      expect {
        # Empty
        @x.init_from_bytes("")
      }.to raise_error(ArgumentError)
      expect {
        # Too long
        @x.init_from_bytes("\x00" * (@x.size + 1))
      }.to raise_error(ArgumentError)
      expect {
        # Too short
        @x.init_from_bytes("\x00" * (@x.size - 1))
      }.to raise_error(ArgumentError)
    end
  end


  describe "#to_h" do
    before :all do
      @x = Inner.new
      @x.init_from_array [3, 2, 1]
      @h = @x.to_h
    end

    it "generates the correct number of elements" do
      expect(@h.length).to eql @x.members.length
    end

    it "generates the correct sequence of keys" do
      expect(@h.keys).to match_array @x.members
    end

    it "generates the correct values" do
      @h.each {|k,v|
        expect(v).to eql @x[k]
      }
    end

    it "recursively encodes nested structures" do
      o = Outer.new
      h = o.to_h
      expect(h[:nested]).to be_a(Hash)
    end
  end


  describe "#init_from_hash" do
    before :all do
      @hash = { :one => 1, :two => 2, :three => 3 }
    end
    
    it "loads the correct values" do
      x = Inner.new
      x.init_from_hash @hash

      expect(x[:one]).to eql 1
      expect(x[:two]).to eql 2
      expect(x[:three]).to eql 3
    end

    it "only alters the specified fields" do
      x = Inner.new
      x.init_from_hash @hash.reject{|z| z == :two}
      
      expect(x[:one]).to eql 1
      expect(x[:two]).to eql 0
      expect(x[:three]).to eql 3
    end

    it "rejects unknown fields" do
      expect {
        h = @hash.merge({ :dummy => 0 })
        x = Inner.new
        x.init_from_hash h
      }.to raise_error(NoMethodError)
    end

    it "can be used to re-constitute the original hash (simple)" do
      h1 = {:one => rand(250),
            :two => rand(250),
            :three => rand(250)
           }
      x = Inner.new
      x.init_from_hash h1
      h2 = x.to_h
      expect(h2).to eql h1
    end

    it "can be used to re-constitute the original structure (simple)" do
      x = Inner.new
      x[:one] = rand(250)
      x[:two] = rand(250)
      x[:three] = rand(250)
      h = x.to_h

      y = Inner.new
      y.init_from_hash h

      expect(y[:one]).to eql x[:one]
      expect(y[:two]).to eql x[:two]
      expect(y[:three]).to eql x[:three]
    end

    it "can be used to re-constitute the original hash (nested)" do
      h1 = {:header => rand(250),
            :nested => {:one => rand(250), :two => rand(250), :three => rand(250)},
            :footer => rand(250)
           }
      x = Outer.new
      x.init_from_hash h1
      h2 = x.to_h
      expect(h2).to eql h1
    end

    it "can be used to re-constitute the original structure (nested)" do
      x = Outer.new
      x[:header] = rand(250)
      x[:footer] = rand(250)
      x[:nested][:one] = rand(250)
      x[:nested][:two] = rand(250)
      x[:nested][:three] = rand(250)
      h = x.to_h

      y = Outer.new
      y.init_from_hash h

      expect(y[:header]).to eql x[:header]
      expect(y[:footer]).to eql x[:footer]
      expect(y[:nested][:one]).to eql x[:nested][:one]
      expect(y[:nested][:two]).to eql x[:nested][:two]
      expect(y[:nested][:three]).to eql x[:nested][:three]
    end

    it "can handle being passed an empty hash" do
      expect {
        x = Inner.new
        h = {}
        x.init_from_hash h
      }.not_to raise_error
    end
  end


  describe "#to_a" do
    before :all do
      @x = Inner.new
      @x[:one] = 1
      @x[:two] = 2
      @x[:three] = 3
      @a = @x.to_a
    end

    it "generates the correct number of elements" do
      expect(@a.length).to eql @x.members.length
    end

    it "generates the correct sequence of values" do
      expect(@a).to match_array [1, 2, 3]
    end

    it "recursively encodes nested structures" do
      o = Outer.new
      a = o.to_a
      expect(a[0]).not_to be_a(::Array)
      expect(a[1]).to     be_a(::Array)
      expect(a[2]).not_to be_a(::Array)
    end
  end


  describe "#init_from_array" do
    before :all do
      @init_data = [rand(250), rand(250), rand(250)]
      @init_data_outer = [rand(250), @init_data, rand(250)]
    end
    
    it "loads the correct values" do
      x = Inner.new
      x.init_from_array [1, 2, 3]

      expect(x[:one]).to eql 1
      expect(x[:two]).to eql 2
      expect(x[:three]).to eql 3
    end

    it "rejects arrays that are the wrong size" do
      x = Inner.new
      expect {
        # empty
        x.init_from_array []
      }.to raise_error(IndexError)
      expect {
        # too small
        x.init_from_array @init_data[0...-1]
      }.to raise_error(IndexError)
      expect {
        # too large
        x.init_from_array @init_data + [5]
      }.to raise_error(IndexError)
    end

    it "can be used to re-constitute the original array (simple)" do
      x = Inner.new
      x.init_from_array @init_data
      expect(x.to_a).to match_array @init_data
    end

    it "can be used to re-constitute the original structure (simple)" do
      x = Inner.new
      x.init_from_array @init_data

      y = Inner.new
      y.init_from_array x.to_a

      expect(y[:one]).to eql x[:one]
      expect(y[:two]).to eql x[:two]
      expect(y[:three]).to eql x[:three]
    end

    it "can be used to re-constitute the original array (nested)" do
      x = Outer.new
      x.init_from_array @init_data_outer
      expect(x.to_a).to match_array @init_data_outer
    end

    it "can be used to re-constitute the original structure (nested)" do
      x = Outer.new
      x.init_from_array @init_data_outer

      y = Outer.new
      y.init_from_array x.to_a

      expect(y[:header]).to eql x[:header]
      expect(y[:footer]).to eql x[:footer]
      expect(y[:nested][:one]).to eql x[:nested][:one]
      expect(y[:nested][:two]).to eql x[:nested][:two]
      expect(y[:nested][:three]).to eql x[:nested][:three]
    end
  end


  describe "Marshal support" do
    describe "#marshal_dump" do
      it "can dump a struct object" do
        x = Inner.new
        expect {
          @dump = Marshal.dump x
        }.not_to raise_error
        expect(@dump).to be_a(String)
      end
    end


    describe "#marshal_load" do
      it "can reconstitute a structure" do
        x = Inner.new
        x[:one] = rand(256)
        x[:two] = rand(256)
        x[:three] = rand(256)
        dump = Marshal.dump x

        y = Marshal.load dump

        expect(y[:one]).to eql x[:one]
        expect(y[:two]).to eql x[:two]
        expect(y[:three]).to eql x[:three]
      end
    end
  end

end
