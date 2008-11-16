# -*- makefile -*-

CFLAGS += -Werror -Wformat
INCFLAGS += -I$(BUILD_DIR) -I$(LIBFFI_BUILD_DIR)/include
CPPFLAGS += -I$(BUILD_DIR) -I$(LIBFFI_BUILD_DIR)/include
LOCAL_LIBS += $(LIBFFI)

FFI_MMAP_EXEC = -DFFI_MMAP_EXEC_WRIT
FFI_CFLAGS = $(FFI_MMAP_EXEC) $(OFLAGS)
BUILD_DIR = $(PWD)/build
LIBFFI_SRC_DIR = $(PWD)/$(srcdir)/libffi
LIBFFI_BUILD_DIR = $(BUILD_DIR)/libffi
LIBFFI = $(LIBFFI_BUILD_DIR)/.libs/libffi_convenience.a
FFI_CONFIGURE = sh $(LIBFFI_SRC_DIR)/configure --disable-static \
	--with-pic=yes --disable-dependency-tracking

$(OBJS):	$(LIBFFI)

clean:	libffi_clean

libffi_clean::
	$(RM) -r $(BUILD_DIR)


