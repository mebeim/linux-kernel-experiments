# NOTE: by default this Makefile assumes each .c file corresponds to a module
#       to be built, use ONLY= to only build specific modules, e.g.
#       ONLY='cpufreq'
# NOTE: for cross-compilation set ARCH=xxx and CROSS_COMPILE=toolchain-prefix-

KDIR := whatever-you-should-replace-this-on-cmdline
PWD  := $(shell pwd)

ifdef ONLY
srcs  := $(ONLY:=.c)
obj-m := $(srcs:.c=.o)
else
srcs  := $(notdir $(filter-out %.mod.c, $(wildcard $M/*.c)))
obj-m := $(srcs:.c=.o)
endif

default: $(srcs)
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
