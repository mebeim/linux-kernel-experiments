/**
 * Dump human readable info from /proc/[pid]/pagemap given a PID and a virtual
 * address, including the physical address.
 *
 * NOTE: refer to `man 5 procfs` for the validity of the bits, some of them only
 *       have a meaning under recent Linux versions (e.g. bit 56 "exclusively
 *       mapped" since Linux 4.2).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>

void dump_pagemap(int pid, unsigned long vaddr) {
	FILE *fp;
	unsigned long info, physaddr;
	char path[128];

	sprintf(path, "/proc/%d/pagemap", pid);

	fp = fopen(path, "rb");
	if (!fp) {
		fprintf(stderr, "Failed to open \"%s\": %s\n", path, strerror(errno));
		_exit(1);
	}

	if (fseek(fp, vaddr >> 9, SEEK_SET)) {
		perror("fseek failed");
		_exit(1);
	}

	if (fread(&info, sizeof(info), 1, fp) != 1) {
		perror("fread failed");
		_exit(1);
	}

	fclose(fp);

	fprintf(
		stderr,
		"Vaddr 0x%lx page 0x%lx\n"
		"  Present?             %hhu\n"
		"  Swapped?             %hhu\n"
		"  File or anon+shared? %hhu\n"
		"  Excl mapped?         %hhu\n"
		"  Soft-dirty PTE?      %hhu\n",
		vaddr, vaddr & ~0xfff,
		(unsigned char)((info >> 63) & 1),
		(unsigned char)((info >> 62) & 1),
		(unsigned char)((info >> 61) & 1),
		(unsigned char)((info >> 56) & 1),
		(unsigned char)((info >> 55) & 1)
	);

	if ((info >> 63) & 1) {
		fprintf(stderr, "  Paddr                0x%lx\n", ((info & ((1UL << 55) - 1)) << 12) | (vaddr & 0xfff));
	} else if ((info >> 62) & 1) {
		fprintf(stderr, "  Swap type            0x%lx\n", info & 0x1f);
		fprintf(stderr, "  Swap offset          0x%lx\n", (info >> 5) & ((1UL << 50) - 1));
	}
}

int main(int argc, char **argv) {
	int pid;
	long tmp;
	unsigned long vaddr;
	char *endp;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s PID VADDR\n", argv[0]);
		return 1;
	}

	errno = 0;
	tmp = strtol(argv[1], &endp, 10);

	if (endp == argv[1] || *endp != '\0' || errno == ERANGE || tmp < 0 || tmp > INT_MAX) {
		fputs("Invalid PID!\n", stderr);
		return 1;
	}

	pid = tmp;
	errno = 0;
	vaddr = strtoul(argv[2], &endp, 0);

	if (endp == argv[2] || *endp != '\0' || errno == ERANGE) {
		fputs("Invalid VADDR!\n", stderr);
		return 1;
	}

	dump_pagemap(pid, vaddr);

	return 0;
}
