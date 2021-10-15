# NOTE: this Makefile assumes each .c file corresponds to a module to be built
# NOTE: for cross-compilation set ARCH=xxx and CROSS_COMPILE=toolchain-prefix-

KDIR := whatever-you-should-replace-this-on-cmdline
PWD  := $(shell pwd)

srcs  := $(notdir $(filter-out %.mod.c, $(wildcard $M/*.c)))
obj-m := $(srcs:.c=.o)

default: $(srcs)
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
