# -*- makefile -*-

$(LIBFFI):		
	@mkdir -p $(LIBFFI_BUILD_DIR)
	@if [ ! -f $(LIBFFI_BUILD_DIR)/Makefile ]; then \
	    echo "Configuring libffi"; \
	    cd $(LIBFFI_BUILD_DIR) && env CC="$(CC)" LD="$(LD)" CFLAGS="$(FFI_CFLAGS)" \
		$(FFI_CONFIGURE) $(LIBFFI_HOST) > /dev/null; \
	fi
	cd $(LIBFFI_BUILD_DIR) && $(MAKE)
