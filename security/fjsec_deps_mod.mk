FJSEC_DEPS_MOD_LIST := $(INSTALLED_KERNEL_TARGET) \
    $(TARGET_OUT)/lib/modules/prima/prima_wlan.ko
FJSEC_MAKE_HEADER := kernel/security/fjsec_make_header.sh

.PHONY: fjsec_rebuild_kernel

ifeq ($(TARGET_BUILD_TYPE),debug)
	FJSEC_SECURITY_DBG :=1
else
	FJSEC_SECURITY_DBG :=0
endif

fjsec_rebuild_kernel: $(FJSEC_DEPS_MOD_LIST)
	@echo "FJSEC make header, and kernel rebuild."
	$(FJSEC_MAKE_HEADER) $(TARGET_OUT)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi-
	$(ACP) $(TARGET_PREBUILT_INT_KERNEL) $(INSTALLED_KERNEL_TARGET)
