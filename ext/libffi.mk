# -*- makefile -*-

$(LIBFFI):		
	@mkdir -p $(LIBFFI_BUILD_DIR)
	@if [ ! -f $(LIBFFI_BUILD_DIR)/Makefile ]; then \
	    echo "Configuring libffi"; \
	    cd $(LIBFFI_BUILD_DIR) && env CC="$(CC)" LD="$(LD)" CFLAGS="$(FFI_CFLAGS)" \
		$(FFI_CONFIGURE) > /dev/null; \
	fi
	$(MAKE) -C $(BUILD_DIR)/libffi
