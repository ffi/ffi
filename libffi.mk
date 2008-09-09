# -*- makefile -*-

OS = $(shell uname -s | tr '[A-Z]' '[a-z]')
LDFLAGS += $(SOFLAGS)
#FFI_MMAP_EXEC = -DFFI_MMAP_EXEC_WRIT
FFI_CFLAGS = $(FFI_MMAP_EXEC) $(OFLAGS)
BUILD_DIR = $(shell pwd)/build
LIBFFI_SRC_DIR = $(shell pwd)/libffi
LIBFFI_BUILD_DIR = $(BUILD_DIR)/libffi
LIBFFI = $(LIBFFI_BUILD_DIR)/.libs/libffi_convenience.a
FFI_CONFIGURE = $(LIBFFI_SRC_DIR)/configure --disable-static \
	--with-pic=yes --disable-dependency-tracking

%.o : %.c
	@mkdir -p $(@D)
	$(CC) -I$(BUILD_DIR) -I$(LIBFFI_BUILD_DIR)/include $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

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
	@$(call build_ffi,ppc)
	
	# Assemble into a FAT (i386, ppc) library
	@mkdir -p $(BUILD_DIR)/libffi/.libs
	env MACOSX_DEPLOYMENT_TARGET=10.4 /usr/bin/libtool -static -o $@ \
            $(BUILD_DIR)/libffi-i386/.libs/libffi_convenience.a \
	    $(BUILD_DIR)/libffi-ppc/.libs/libffi_convenience.a
	@mkdir -p $(LIBFFI_BUILD_DIR)/include
	$(RM) $(LIBFFI_BUILD_DIR)/include/ffi.h
	@( \
		printf "#if defined(__i386__)\n"; \
		printf "#include \"libffi-i386/include/ffi.h\"\n"; \
		printf "#elif defined(__ppc__)\n"; \
		printf "#include \"libffi-ppc/include/ffi.h\"\n";\
		printf "#endif\n";\
	) > $(LIBFFI_BUILD_DIR)/include/ffi.h
	@( \
		printf "#if defined(__i386__)\n"; \
		printf "#include \"libffi-i386/include/ffitarget.h\"\n"; \
		printf "#elif defined(__ppc__)\n"; \
		printf "#include \"libffi-ppc/include/ffitarget.h\"\n";\
		printf "#endif\n";\
	) > $(LIBFFI_BUILD_DIR)/include/ffitarget.h
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

libffi_clean::
	$(RM) -r $(BUILD_DIR)
