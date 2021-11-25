# NOTE: by default this Makefile assumes each .c file corresponds to a module
#       to be built, use ONLY= to only build specific modules, e.g.
#       ONLY='cpufreq'

KDIR := whatever-you-should-replace-this-on-cmdline
PWD  := $(shell pwd)
ARCH := arm64
CROSS_COMPILE := aarch64-linux-gnu-

ifdef ONLY
srcs  := $(ONLY:=.c)
obj-m := $(srcs:.c=.o)
else
srcs  := $(notdir $(filter-out %.mod.c, $(wildcard $M/*.c)))
obj-m := $(srcs:.c=.o)
endif

default: $(srcs)
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
