# -*- makefile -*-
OS ?= $(shell uname -s | tr '[A-Z]' '[a-z]')
CPU ?= $(shell uname -m | sed -e 's/i[345678]86/i386/')
MODEL = 32 # Default to 32bit compiles
PLATFORM = $(CPU)-$(OS)
JDK_HOME=$(shell if [ -d $(JAVA_HOME)/include ];then echo $(JAVA_HOME); else echo $(JAVA_HOME)/..; fi)

JNI_DIR = $(shell pwd)
SRC_DIR = libtest
BUILD_DIR ?= build
TEST_BUILD_DIR = $(BUILD_DIR)/libtest
# Set defaults to unix (linux/solaris/bsd)
PREFIX = lib
LIBEXT = so
JNIEXT = so

export MACOSX_DEPLOYMENT_TARGET=10.4

CCACHE=$(shell type -p ccache)

TEST_OBJS = Benchmark.o BufferTest.o ClosureTest.o LastErrorTest.o NumberTest.o \
	PointerTest.o ReferenceTest.o StringTest.o StructTest.o
TEST_OBJS := $(patsubst %.o, $(TEST_BUILD_DIR)/%.o, $(TEST_OBJS))
	
#
# Compiler/linker flags from:
#   http://weblogs.java.net/blog/kellyohair/archive/2006/01/compilation_of_1.html
JFLAGS = -fno-omit-frame-pointer -fno-strict-aliasing -DNDEBUG
OFLAGS = -O2 $(JFLAGS)
WFLAGS = -W -Werror -Wall -Wno-unused -Wno-parentheses
PICFLAGS = -fPIC -pthread
SOFLAGS = -shared -mimpure-text -Wl,-O1
LDFLAGS += $(SOFLAGS)

JDK_INCLUDES= -I"$(JDK_HOME)/include" -I"$(JDK_HOME)/include/$(OS)"
IFLAGS = -I"$(BUILD_DIR)" -I"$(JFFI_SRC_DIR)" -I"$(LIBFFI_BUILD_DIR)/include"
CFLAGS = $(OFLAGS) $(WFLAGS) $(IFLAGS) $(PICFLAGS) $(JDK_INCLUDES)
CFLAGS += -D_REENTRANT -D_LARGEFILE64_SOURCE -D_GNU_SOURCE

ifeq ($(OS), win32)
	CC += -mno-cygwin
	LDFLAGS += -mno-cygwin -Wl,--add-stdcall-alias
endif
ifeq ($(OS), darwin)
  CFLAGS += -arch i386 -arch ppc -arch x86_64 -isysroot /Developer/SDKs/MacOSX10.4u.sdk -DTARGET_RT_MAC_CFM=0
  LDFLAGS = -arch i386 -arch ppc -arch x86_64 -dynamiclib -framework JavaVM \
	-Wl,-syslibroot,$(SDKROOT) -mmacosx-version-min=10.4
  LIBEXT = dylib
  JNIEXT = jnilib
  FFI_CFLAGS += -isysroot /Developer/SDKs/MacOSX10.4u.sdk
  PICFLAGS =
  SOFLAGS =
endif

ifeq ($(OS), linux)
  SOFLAGS += -Wl,-soname,lib$(LIBNAME).so
endif

ifeq ($(CPU), sparcv9)
  MODEL = 64
endif

ifeq ($(CPU), amd64)
  MODEL = 64
endif

ifeq ($(MODEL), 64)
  CC += -m64
  LD += -m64
endif

LIBTEST = $(BUILD_DIR)/$(PREFIX)$(LIBNAME)test.$(LIBEXT)

all:	$(LIBTEST)

$(TEST_BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBTEST):  $(TEST_OBJS)
	$(CC) -o $@ $(LDFLAGS) $(TEST_OBJS) -lm

clean::
	# nothing to do - ant will delete the build dir
	
