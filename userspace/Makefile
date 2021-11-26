BINS = $(patsubst %.c,%,$(wildcard *.c))

.PHONY: all clean

all: $(BINS)

clean:
	rm -f $(BINS)

alias_existing_page: alias_existing_page.c
	$(CC) $^ -o $@

inherit_capability: inherit_capability.c
	$(CC) $^ -o $@ -lcap

mmap_devzero_wtf: mmap_devzero_wtf.c
	$(CC) $^ -o $@

pagemap: pagemap.c
	$(CC) $^ -o $@

test_scaling_governor: test_scaling_governor.c
	$(CC) $^ -o $@

x86_rtm_page_fault: x86_rtm_page_fault.c
	$(CC) $^ -o $@ -mrtm
