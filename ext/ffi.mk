OS=$(shell uname -s)
ARCH=$(shell uname -m | sed -e 's/i[345678]86/i386/')
CFLAGS += -DOS=\"$(OS)\"
INCFLAGS += -I$(BUILD_DIR) -I$(LIBFFI_BUILD_DIR)/include
include $(srcdir)/libffi.mk

$(OBJS):	$(LIBFFI)
LOCAL_LIBS += $(LIBFFI)

clean:	libffi_clean

