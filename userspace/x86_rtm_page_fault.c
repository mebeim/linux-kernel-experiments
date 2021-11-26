/**
 * Use x86 transactional memory to detect a page fault when writing at the
 * specified address, assuming it's a valid address.
 *
 * NOTE: check RTM support by looking at /proc/cpuinfo for the 'rtm' flag,
 *       compile using GCC *without optimizations* and with the `-mrtm` flag.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <immintrin.h>

static int page_dirty(volatile unsigned char *p) {
	unsigned status;

	if ((status = _xbegin()) == _XBEGIN_STARTED) {
		*p = 0;
		_xend();

		/* Transaction successfully ended => no context switch happened to
		 * copy page into virtual memory of the process => page was dirty.
		 */
		return 1;
	} else {
		/* Transaction aborted => page fault happened and context was switched
		 * to copy page into virtual memory of the process => page wasn't dirty.
		 */
		return 0;
	}

	/* Should not happen! */
	return -1;
}

int main(void) {
	volatile unsigned char *addr;

	addr = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		perror("parent: mmap failed");
		return 1;
	}

	// Write to trigger initial page fault and actually reserve memory
	*addr = 123;

	fprintf(stderr, "Initial state : %d\n", page_dirty(addr));
	fputs("----- fork -----\n", stderr);

	if (fork()) {
		fprintf(stderr, "Parent before : %d\n", page_dirty(addr));

		// Read (should NOT trigger Copy on Write)
		*addr;

		fprintf(stderr, "Parent after R: %d\n", page_dirty(addr));

		// Write (should trigger Copy on Write)
		*addr = 123;

		fprintf(stderr, "Parent after W: %d\n", page_dirty(addr));
	} else {
		fprintf(stderr, "Child before  : %d\n", page_dirty(addr));

		// Read (should NOT trigger Copy on Write)
		*addr;

		fprintf(stderr, "Child after R : %d\n", page_dirty(addr));

		// Write (should trigger Copy on Write)
		*addr = 123;

		fprintf(stderr, "Child after W : %d\n", page_dirty(addr));
	}

	return 0;
}
