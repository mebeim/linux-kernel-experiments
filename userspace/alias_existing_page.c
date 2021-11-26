/**
 * Get the physical address of an existing virtual memory page and map it,
 * effectively creating an "alias" for an existing page at a different virtual
 * addres.
 *
 * Sparkled from: https://stackoverflow.com/questions/67781437
 */

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
	FILE *fp;
	unsigned long addr, info, physaddr, val;
	long off;
	int fd;
	void *mem;
	void *orig_mem;

	// Suppose that this is the existing page you want to "alias"
	orig_mem = mmap(NULL, 0x1000, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	if (orig_mem == MAP_FAILED) {
		perror("mmap orig_mem failed");
		return 1;
	}

	// Write a dummy value just for testing
	*(unsigned long *)orig_mem = 0x1122334455667788UL;

	// Lock the page to prevent it from being swapped out
	if (mlock(orig_mem, 0x1000)) {
		perror("mlock orig_mem failed");
		return 1;
	}

	fp = fopen("/proc/self/pagemap", "rb");
	if (!fp) {
		perror("Failed to open \"/proc/self/pagemap\"");
		return 1;
	}

	addr = (unsigned long)orig_mem;
	off  = addr / 0x1000 * 8;

	if (fseek(fp, off, SEEK_SET)) {
		perror("fseek failed");
		return 1;
	}

	// Get its information from /proc/self/pagemap
	if (fread(&info, sizeof(info), 1, fp) != 1) {
		perror("fread failed");
		return 1;
	}

	physaddr = (info & ((1UL << 55) - 1)) << 12;

	printf("Value: %016lx\n", info);
	printf("Physical address: 0x%016lx\n", physaddr);

	// Ensure page is in RAM, should be true since it was mlock'd
	if (!(info & (1UL << 63))) {
		fputs("Page is not in RAM? Strange! Aborting.\n", stderr);
		return 1;
	}

	fd = open("/dev/mem", O_RDONLY);
	if (fd == -1) {
		perror("open(\"/dev/mem\") failed");
		return 1;
	}

	mem = mmap(NULL, 0x1000, PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, fd, physaddr);
	if (mem == MAP_FAILED) {
		perror("Failed to open \"/dev/mem\"");
		return 1;
	}

	// Now `mem` is effecively referring to the same physical page that
	// `orig_mem` refers to.

	// Try reading 8 bytes (note: this will just return 0 if
	// CONFIG_STRICT_DEVMEM=y).
	val = *(unsigned long *)mem;

	printf("Read 8 bytes at physaddr 0x%016lx: %016lx\n", physaddr, val);

	return 0;
}
