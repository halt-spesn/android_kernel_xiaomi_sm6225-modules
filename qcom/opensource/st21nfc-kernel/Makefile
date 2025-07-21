obj-$(CONFIG_NFC_ST21NFC)	+= st21nfc.o

KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
M ?= $(shell pwd)

KBUILD_OPTIONS += CONFIG_NFC_ST21NFC=m

ccflags-y := -I$(KERNEL_SRC)/../sm6225-modules/st21nfc-kernel

modules modules_install clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(KBUILD_OPTIONS) W=1 $(@)
