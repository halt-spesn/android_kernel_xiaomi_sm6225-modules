obj-$(CONFIG_FINGERPRINT_FPC1020) += fpc1020_platform_tee.o

KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
M ?= $(shell pwd)

KBUILD_OPTIONS += CONFIG_FINGERPRINT_FPC1020=m

ccflags-y := -I$(KERNEL_SRC)/../sm6225-modules/fpc-kernel

modules modules_install clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(KBUILD_OPTIONS) W=1 $(@)
