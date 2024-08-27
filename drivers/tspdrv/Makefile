#=========================================================================
# File:
#    Makefile
#
# Description: 
#    Makefile for TouchSense Linux Kernel Module
#    Targets Kernel 2.6
# =========================================================================
#  COPYRIGHT(C) FUJITSU LIMITED 2013
# =========================================================================

ifeq ($(KERNEL_ROOT),)
    override KERNEL_ROOT = /usr/src/linux
endif

obj-$(CONFIG_TSPDRV) += tspdrv.o tspdrv_i2c.o

all:
	@$(MAKE) -C $(KERNEL_ROOT) M=$(shell pwd) modules

clean:
	@$(MAKE) -C $(KERNEL_ROOT) M=$(shell pwd) clean
