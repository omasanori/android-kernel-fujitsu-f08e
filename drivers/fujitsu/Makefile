#
# Makefile for fujitsu devices that really don't fit anywhere else.
#

include $(srctree)/../fjfeat/FjFeatMac.mk

# FUJITSU:2013-04-17 COMPASS mod start
ifeq ("$(FJFEAT_PRODUCT)", "$(FJFEAT_PRODUCT_FJDEV004)")
obj-y  += compass/
else
ifeq ("$(FJFEAT_PRODUCT)", "$(FJFEAT_PRODUCT_FJDEV005)")
obj-y  += compass/
endif
endif
# FUJITSU:2013-04-17 COMPASS mod end
obj-y		+= ovp/
obj-y		+= mkdrv/
obj-y 		+= ftdtv_drv/
obj-y		+= walkmotion/
obj-$(CONFIG_IRDA_FJ) += irda/
