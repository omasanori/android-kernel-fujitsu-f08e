#
# Makefile for the Silicon Image 8334 MHL TX device driver
#
# example invocations:	
#	make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- sii8334_clean sii8334_debug
#	make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- sii8334_clean sii8334_release
#	make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi- sii8334_clean sii8334_clean

BUILDNUM=61
ifneq ($(KERNELRELEASE),) #if kernel 
# kbuild part of makefile

ccflags-y := -I$(src)/include
ccflags-y += -I$(src)/platform
ccflags-y += -I$(src)/platform/hal
ccflags-y += -I$(src)/component/mhl_tx
ccflags-y += -I$(src)/driver/cra_drv
ccflags-y += -I$(src)/driver/sii8334_drv
ccflags-y += -I$(src)/linux_kernel/kernel_module
ccflags-y += -I$(src)/platform/api
ccflags-y += -I$(src)/platform/include
ccflags-y += -DMAKE_8334_DRIVER
ccflags-y += -DC99_VA_ARG_SUPPORT
ccflags-y += -DENABLE_TX_DEBUG_PRINT
ccflags-y += -DENABLE_CBUS_DEBUG_PRINT
ccflags-y += -DBUILDNUM=$(BUILDNUM)
#ccflags-y += -DDEBUG
ccflags-y += -DFJ_DEBUG

obj-$(CONFIG_SII8334_MHL_TX) += sii8334drv.o
#obj-m += sii8334drv.o
#----------------- App layer --------------------------------------
sii8334drv-objs  += linux_kernel/kernel_module/mhl_linuxdrv_main.o
sii8334drv-objs  += linux_kernel/public/mhl_linuxdrv_ioctl.o
sii8334drv-objs  += linux_kernel/public/mhl_linuxdrv_open_close.o
#----------------- Component layer --------------------------------
sii8334drv-objs  += component/mhl_tx/si_mhl_tx.o
sii8334drv-objs  += component/mhl_tx/si_mhl_tx_rcp_table.o
#----------------- Platform layer ---------------------------------
sii8334drv-objs  += platform/osal/si_osdebug.o
sii8334drv-objs  += platform/osal/sii_osal_linux_timer.o
sii8334drv-objs  += platform/hal/sii_hal_linux_init.o
sii8334drv-objs  += platform/hal/sii_hal_linux_i2c.o
sii8334drv-objs  += platform/hal/sii_hal_linux_isr.o
sii8334drv-objs  += platform/hal/sii_hal_linux_timer.o
sii8334drv-objs  += platform/hal/sii_hal_linux_gpio.o
#----------------- Driver layer --------------------------------
sii8334drv-objs  += driver/cra_drv/si_cra.o
sii8334drv-objs  += driver/sii8334_drv/si_cra_cfg.o
sii8334drv-objs  += driver/sii8334_drv/si_drv_mhl_tx.o

else

# Normal Makefile

#KERNELDIR=~/src/linux-2.6.36
#ARCH=arm

KERNELDIR=~/Intel_Medfield/Medfield/dev/android-gingerbread/out/target/product/mfld_cdk/kernel_build/
#ARCH=arm
ARCH=x86

PWD := $(shell pwd)


.PHONY: sii8334_clean


sii8334_release:
	make -C $(KERNELDIR) M=$(PWD) CONFIG_SII8334_MHL_TX=m modules
	$(CROSS_COMPILE)strip --strip-debug sii8334drv.ko

sii8334_debug:
	make -C $(KERNELDIR) M=$(PWD) CONFIG_SII8334_MHL_TX=m CONFIG_DEBUG_DRIVER=y modules

sii8334_clean:
	make -C $(KERNELDIR) M=$(PWD) CONFIG_SII8334_MHL_TX=m clean
endif
