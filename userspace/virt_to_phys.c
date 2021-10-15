/**
 * Find the corresponding physical address given a PID and a valid virtual
 * address in its VA space.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

int main(int argc, char **argv) {
	char path[128];
	FILE *fp;
	char *endp;
	long off;
	unsigned long addr;
	unsigned long info;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s PID ADDRESS\n", argv[0]);
		return 1;
	}

	errno = 0;
	addr = strtoul(argv[2], &endp, 0);

	if (endp == argv[1] || *endp != '\0' || errno == ERANGE) {
		fputs("Invalid ADDRESS\n", stderr);
		return 1;
	}

	if (snprintf(path, sizeof(path), "/proc/%s/pagemap", argv[1]) < 0) {
		perror("snprintf failed");
		fputs("Invalid PID?\n", stderr);
		return 1;
	}

	fp = fopen(path, "rb");
	if (!fp) {
		fprintf(stderr, "Failed to open '%s': %s\n", path, strerror(errno));
		return 1;
	}

	off = addr / 0x1000 * 8;

	if (fseek(fp, off, SEEK_SET)) {
		perror("fseek failed");
		return 1;
	}

	if (fread(&info, sizeof(info), 1, fp) != 1) {
		perror("fread failed");
		return 1;
	}

	printf("Value: %016lx\n", info);
	printf("Physical address: 0x%016lx\n", (info & ((1UL << 55) - 1)) << 12);

	return 0;
}
