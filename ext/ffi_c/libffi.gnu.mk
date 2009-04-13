# -*- makefile -*-
#
#  Common definitions for all systems that use GNU make
#
INCFLAGS += -I$(BUILD_DIR) -I$(LIBFFI_BUILD_DIR)/include
CPPFLAGS += -I$(BUILD_DIR) -I$(LIBFFI_BUILD_DIR)/include
LOCAL_LIBS += $(LIBFFI)

FFI_CFLAGS = $(FFI_MMAP_EXEC)
BUILD_DIR := $(shell pwd)
ifeq ($(srcdir),.)
  LIBFFI_SRC_DIR := $(shell pwd)/libffi
else
  LIBFFI_SRC_DIR := $(srcdir)/libffi
endif
LIBFFI_BUILD_DIR = $(BUILD_DIR)/libffi
LIBFFI = $(LIBFFI_BUILD_DIR)/.libs/libffi_convenience.a
FFI_CONFIGURE = $(LIBFFI_SRC_DIR)/configure --disable-static \
	--with-pic=yes --disable-dependency-tracking

$(OBJS):	$(LIBFFI)

