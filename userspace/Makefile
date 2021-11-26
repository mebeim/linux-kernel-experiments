BINS = $(patsubst %.c,%,$(wildcard *.c))
CFLAGS = -Wall -Wextra -pedantic -O3

.PHONY: all clean

all: $(BINS)

clean:
	rm -f $(BINS)

alias_existing_page: alias_existing_page.c
	$(CC) $(CFLAGS) $^ -o $@

inherit_capability: inherit_capability.c
	$(CC) $(CFLAGS) $^ -o $@ -lcap

mmap_devzero_wtf: mmap_devzero_wtf.c
	$(CC) $(CFLAGS) $^ -o $@

pagemap: pagemap.c
	$(CC) $(CFLAGS) $^ -o $@

test_scaling_governor: test_scaling_governor.c
	$(CC) $(CFLAGS) $^ -o $@

x86_rtm_page_fault: x86_rtm_page_fault.c
	$(CC) $(CFLAGS) -O0 $^ -o $@ -mrtm
