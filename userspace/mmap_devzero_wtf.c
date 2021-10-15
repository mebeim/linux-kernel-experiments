/**
 * There seems to be a strange edge case when mmapping /dev/zero (maybe even
 * other devices?) where if you map O_RDWR with MAP_SHARED at an offset that is
 * not 0 you get a SIGBUS when trying to write to the mapping.
 *
 * Tested on: Linux Marco-Debian 4.9.0-16-amd64 #1 SMP Debian 4.9.272-2 (2021-07-19) x86_64 GNU/Linux
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>

int main(void) {
	int fd;
	void *mem;
	unsigned char data;

	/* WTF is going on?
	 *
	 * open_mode  mmap_flags   mmap_offset  RESULT
	 * O_RDONLY   MAP_PRIVATE  0x0          OK
	 * O_RDONLY   MAP_PRIVATE  0x1000       OK
	 * O_RDONLY   MAP_SHARED   0x0          OK
	 * O_RDONLY   MAP_SHARED   0x1000       OK
	 * O_RDWR     MAP_PRIVATE  0x0          OK
	 * O_RDWR     MAP_PRIVATE  0x1000       OK
	 * O_RDWR     MAP_SHARED   0x0          OK
	 * O_RDWR     MAP_SHARED   0x1000       SIGBUS
	 */

	int open_flags    = O_RDWR;
	int mmap_flags    = MAP_SHARED;
	off_t mmap_offset = 0x1000;

	fd = open("/dev/zero", open_flags);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	mem = mmap(NULL, 0x1000, PROT_READ, mmap_flags, fd, mmap_offset);
	if (mem == MAP_FAILED) {
		perror("mmap");
		return 1;
	}

	fprintf(stderr, "Memory mapped at %p.\n", mem);

	// SIGBUS here IFF open_flags = O_RDWR, mmap_flags = MAP_SHARED, mmap_offset = 0x1000
	data = *(unsigned char *)mem;

	fprintf(stderr, "Read @ %p: 0x%02hhx\n", mem, data);

	return 0;
}
