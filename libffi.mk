# -*- makefile -*-

OS = $(shell uname -s | tr '[A-Z]' '[a-z]')
CPU ?= $(shell uname -p)
MODEL = 32 # Default to 32bit compiles
LDFLAGS += $(SOFLAGS)
#FFI_MMAP_EXEC = -DFFI_MMAP_EXEC_WRIT
FFI_CFLAGS = $(FFI_MMAP_EXEC) $(OFLAGS)
BUILD_DIR = build
LIBFFI_SRC_DIR = $(shell pwd)/libffi
LIBFFI_BUILD_DIR = $(BUILD_DIR)/libffi
LIBFFI = $(LIBFFI_BUILD_DIR)/.libs/libffi_convenience.a

FFI_CONFIGURE = $(LIBFFI_SRC_DIR)/configure --disable-static \
	--with-pic=yes --disable-dependency-tracking

all:	$(LIBFFI)
	
ifeq ($(OS), darwin)
$(BUILD_DIR)/%.o:	$(SRC_DIR)/%.c $(JFFI_SRC_DIR)/jffi.h
	@mkdir -p $(@D)
	$(CC) -arch i386 -I$(BUILD_DIR)/libffi-i386/include $(CFLAGS) -c $< -o $@.i386
	$(CC) -arch x86_64 -I$(BUILD_DIR)/libffi-x86_64/include $(CFLAGS) -c $< -o $@.x86_64
	lipo -create -output $@ -arch x86_64 $@.x86_64 -arch i386 $@.i386
else
$(BUILD_DIR)/%.o : $(SRC_DIR)/%.c jffi.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
endif

ifeq ($(OS), darwin)
build_ffi = \
	mkdir -p $(BUILD_DIR)/libffi-$(1); \
	(if [ ! -f $(BUILD_DIR)/libffi-$(1)/Makefile ]; then \
	    echo "Configuring libffi for $(1)"; \
	    cd $(BUILD_DIR)/libffi-$(1) && \
	      env CFLAGS="-arch $(1) $(FFI_CFLAGS)" LDFLAGS="-arch $(1)" \
		$(FFI_CONFIGURE) --host=$(1)-apple-darwin > /dev/null; \
	fi); \
	env MACOSX_DEPLOYMENT_TARGET=10.4 $(MAKE) -C $(BUILD_DIR)/libffi-$(1)
	
$(LIBFFI):
	@$(call build_ffi,i386)
	@$(call build_ffi,x86_64)
	
	# Assemble into a FAT (i386, x86_64) library
	@mkdir -p $(BUILD_DIR)/libffi/.libs
	env MACOSX_DEPLOYMENT_TARGET=10.4 /usr/bin/libtool -static -o $@ \
            $(BUILD_DIR)/libffi-i386/.libs/libffi_convenience.a \
	    $(BUILD_DIR)/libffi-x86_64/.libs/libffi_convenience.a
            
else
$(LIBFFI):		
	@mkdir -p $(BUILD_DIR)/libffi
	@if [ ! -f $(BUILD_DIR)/libffi/Makefile ]; then \
	    echo "Configuring libffi"; \
	    cd $(BUILD_DIR)/libffi && env CC="$(CC)" LD="$(LD)" CFLAGS="$(FFI_CFLAGS)" \
		$(FFI_CONFIGURE) > /dev/null; \
	fi
	$(MAKE) -C $(BUILD_DIR)/libffi
endif

clean::
	$(RM) -r $(BUILD_DIR)
