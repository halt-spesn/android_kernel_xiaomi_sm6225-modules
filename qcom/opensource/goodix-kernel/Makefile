GOODIX_MODULE_NAME = goodix_fp
obj-$(CONFIG_FINGERPRINT_GOODIX_FOD) += $(GOODIX_MODULE_NAME).o
$(GOODIX_MODULE_NAME)-objs := gf_spi.o platform.o netlink.o

KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
M ?= $(shell pwd)

KBUILD_OPTIONS += CONFIG_FINGERPRINT_GOODIX_FOD=m

ccflags-y := -I$(KERNEL_SRC)/../sm6225-modules/goodix-kernel

modules modules_install clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(KBUILD_OPTIONS) W=1 $(@)
