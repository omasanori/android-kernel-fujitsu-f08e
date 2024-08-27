#
# Makefile for the Linux NFC subsystem.
#

# FUJITSU:2012-12-02 NFC start
#obj-$(CONFIG_NFC) += nfc.o
# FUJITSU:2012-12-02 NFC end
obj-$(CONFIG_NFC_NCI) += nci/

# FUJITSU:2012-12-02 NFC start
#nfc-objs := core.o netlink.o af_nfc.o rawsock.o
# FUJITSU:2012-12-02 NFC end
nfc-$(CONFIG_NFC_LLCP)	+= llcp/llcp.o llcp/commands.o llcp/sock.o
