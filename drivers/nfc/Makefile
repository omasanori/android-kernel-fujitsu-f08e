#
# Makefile for nfc devices
#

obj-$(CONFIG_PN544_NFC)		+= pn544.o
obj-$(CONFIG_NFC_PN533)		+= pn533.o
obj-$(CONFIG_NFC_WILINK)	+= nfcwilink.o
obj-$(CONFIG_BCM2079X)		+= bcm2079x-i2c.o

obj-$(CONFIG_NFC_FJ)		+= snfc_hw.o snfc_cen.o snfc_pon.o snfc_rfs.o snfc_intu.o snfc_hsel.o snfc_available_poll.o snfc_arbiter.o snfc_power_observe.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
