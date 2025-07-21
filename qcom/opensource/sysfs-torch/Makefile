obj-$(CONFIG_FLASHLIGHT_DRIVER) += flashlight.o

KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
M ?= $(shell pwd)

KBUILD_OPTIONS += CONFIG_FLASHLIGHT_DRIVER=m

ccflags-y := -I$(KERNEL_SRC)/../sm6225-modules/sysfs-torch

modules modules_install clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(KBUILD_OPTIONS) W=1 $(@)
