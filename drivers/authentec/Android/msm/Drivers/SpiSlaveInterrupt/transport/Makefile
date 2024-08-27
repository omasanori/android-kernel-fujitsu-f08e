

# Primary definitions to change in this file:
# KDIR     - where the kernel source is located
# TOOLDIR  - where the compiler is located
# TOOL_PREFIX - the tool prefix which is toolchain specific

# Location of a configured kernel tree
#FUJITSU:2011-12-12 FingerPrintSensor mod start
KDIR = $(PWD)/../../../../../../../..
#FUJITSU:2011-12-12 FingerPrintSensor mod end

# installation directory for our cross compiler's binaries:
#FUJITSU:2011-12-12 FingerPrintSensor mod start
TOOLDIR=/opt/arm-2010q1/bin
#FUJITSU:2011-12-12 FingerPrintSensor mod end

# prefix for cross compiler tool eecutable names:
TOOL_PREFIX=arm-none-linux-gnueabi-


# KERNEL_VERSION = 2.4 or 2.6
# -- this is to allow the 2.6 LKM build system to be used for the device driver
KERNEL_VERSION = 2.6


# set ARCH to the architecture prefix of the compiler name
ARCH=arm

# set OS to the operating system prefix of the compiler name
OS=linux

# prefix to all cross compiler tools
#FUJITSU:2011-12-12 FingerPrintSensor mod start
CROSS_COMPILE=${TOOL_PREFIX}
#FUJITSU:2011-12-12 FingerPrintSensor mod end

# If the kernel has any required compiler flags, include them here:
#KERNEL_CFLAGS = -mapcs -mno-sched-prolog -mabi=aapcs-linux -mno-thumb-interwork -march=armv6 -mtune=arm1136j-s -msoft-float

# This initial CFLAGS will be a part of every CFLAGS setting
#FUJITSU:2011-12-12 FingerPrintSensor add start
CONFIG_AUTH_FP=y
#FUJITSU:2011-12-12 FingerPrintSensor add end
ifndef CONFIG_AUTH_FP
#FUJITSU:2011-12-12 FingerPrintSensor mod start
EXTRA_CFLAGS = -Wall -Wextra
#FUJITSU:2011-12-12 FingerPrintSensor mod end
endif



# definitions for all tools
AS      =       ${CROSS_COMPILE}as
CC      =       ${CROSS_COMPILE}gcc
CPP     =       ${CROSS_COMPILE}g++
CXX     =       ${CROSS_COMPILE}g++
LD      =       ${CROSS_COMPILE}ld
LDD     =       ${CROSS_COMPILE}ldd
NM      =       ${CROSS_COMPILE}nm
OBJCOPY =       ${CROSS_COMPILE}objcopy
OBJDUMP =       ${CROSS_COMPILE}objdump
RANLIB  =       ${CROSS_COMPILE}ranlib
STRIP   =       ${CROSS_COMPILE}strip

XCC = ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

#path of AuthenTec provided header files
AUTH_INCLUDES = \
	-I$(PWD)/../../Include \
	-I$(PWD)/../../../../../Include/transport



FEATUREFLAG = 


ALL_DEFS = -D__KERNEL__ -DMODULE -DNO_DEBUG $(FEATUREFLAG)\
						-I$(PWD) \
           -I$(KDIR)/include $(AUTH_INCLUDES) \
           -I$(KDIR)/arch/arm/include -I$(KDIR) 

DEBUG = -O2
#DEBUG = -g3 -O0 -gdwarf-2 -mapcs-frame

ifndef CONFIG_AUTH_FP
EXTRA_CFLAGS += $(DEBUG) \
	-Wall \
	$(ALL_DEFS) \
	-fno-common
endif

DRIVER_OBJS = authtransport.o authaes.o authos.o authbuffer.o authgpio.o authhalslaveinterrupt.o authisrinterrupt.o authioslave.o 

SOURCE_FILES = authtransport.c authaes.c authos.c authbuffer.c authgpio.c authhalslaveinterrupt.c authisrinterrupt.c authioslave.c 


all: authtransportdrv



ifeq ($(KERNEL_VERSION),2.4)
authtransportdrv: $(DRIVER_OBJS)
	$(LD) -r -o authtransportdrv.o $(DRIVER_OBJS) \	
else

authtransportdrv: $(SOURCE_FILES) 

endif
	$(MAKE) KBUILD_NOPEDANTIC=1 -C $(KDIR) SUBDIRS=$(PWD) modules $(XCC)
	@echo
	@echo
	@echo "                    ***************************************"
	@echo "                    * UDM transport driver build complete *"
	@echo "                    ***************************************"
	@echo
	@echo



clean:
	rm -rf .tmp_versions .*.cmd *.ko *.mod.c *.o *.symvers *.order

ifndef CONFIG_AUTH_FP
ifneq ($(KERNELRELEASE),)

obj-m:=authtransportdrv.o 
authtransportdrv-y := $(DRIVER_OBJS)

else
PWD:= $(shell pwd)
default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules $(XCC)
endif

else #build driver into kernel image or build module from within kerel tree using kernel build system.
#FUJITSU:2011-12-12 FingerPrintSensor mod start
ccflags-y := -I$(src)/../../../../../Include -I$(src)/../../Include -I$(src) -I$(src)/../../../../../Include/transport
#FUJITSU:2011-12-12 FingerPrintSensor mod end
obj-$(CONFIG_AUTH_FP) := authtransportdrv.o 
authtransportdrv-y := $(DRIVER_OBJS)

endif #CONFIG_AUTH_FP


