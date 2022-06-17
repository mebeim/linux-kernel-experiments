/**
 * Dump human readable info from /proc/[pid]/pagemap, /proc/kpageflags and
 * /proc/kpagecount given a PID and a virtual address OR a physical address.
 *
 * NOTE: Refer to `man 5 procfs` for the validity of the bits, some of them only
 *       have a meaning under recent Linux versions. Refer to
 *       Documentation/admin-guide/mm/pagemap.rst for the meaning of the bits.
 * NOTE: Undocumented KPF_* flags available for "kernel hacking assistance"
 *       should not be relied upon: check source for the running kernel's
 *       version to make sure they are correct.
 *
 * Changelog:
 *
 * v0.3.1: Use "self" instead of -1 for self-inspection.
 * v0.3:   Support self-inspection passing -1 as pid. Support KPF_PGTABLE, plus
 *         undocumented KPF flags available for "kernel hacking assistance".
 *         Report number of times a page is mapped, obtained through
 *         /proc/kpagecount. Fix wrong calculation of file offset when reading
 *         pagemap.
 * v0.2:   Support more flags from /proc/[pid]/pagemap and also dump flags from
 *         /proc/kpageflags. Support physical addresses (no PID specified).
 * v0.1:   Initial version, only reading /proc/[pid]/pagemap.
 *
 */

#define _XOPEN_SOURCE 500
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

#define PAGE_SHIFT (12)
#define PAGE_MASK  (~((1UL << PAGE_SHIFT) - 1))

#define PM_PRESENT        (1UL << 63)
#define PM_SWAP           (1UL << 62)
#define PM_FILE           (1UL << 61) // since Linux 3.5
#define PM_UFFD_WP        (1UL << 57) // since Linux 5.17
#define PM_MMAP_EXCLUSIVE (1UL << 56) // since Linux 4.2
#define PM_SOFT_DIRTY     (1UL << 55) // since Linux 3.11
#define PM_PFRAME_MASK    ((1UL << 55) - 1)

#define PM_SWAP_TYPE_MASK    0x1f
#define PM_SWAP_OFFSET_SHIFT 5
#define PM_SWAP_OFFSET_MASK  (((1UL << 50) - 1) << PM_SWAP_OFFSET_SHIFT)

#define PM_FLAGS (PM_PRESENT|PM_SWAP|PM_FILE|PM_UFFD_WP|PM_MMAP_EXCLUSIVE|PM_SOFT_DIRTY)

// include/uapi/linux/kernel-page-flags.h
#define KPF_LOCKED        (1UL << 0 )
#define KPF_ERROR         (1UL << 1 )
#define KPF_REFERENCED    (1UL << 2 )
#define KPF_UPTODATE      (1UL << 3 )
#define KPF_DIRTY         (1UL << 4 )
#define KPF_LRU           (1UL << 5 )
#define KPF_ACTIVE        (1UL << 6 )
#define KPF_SLAB          (1UL << 7 )
#define KPF_WRITEBACK     (1UL << 8 )
#define KPF_RECLAIM       (1UL << 9 )
#define KPF_BUDDY         (1UL << 10)
#define KPF_MMAP          (1UL << 11) // since Linux 2.6.31
#define KPF_ANON          (1UL << 12) // since Linux 2.6.31
#define KPF_SWAPCACHE     (1UL << 13) // since Linux 2.6.31
#define KPF_SWAPBACKED    (1UL << 14) // since Linux 2.6.31
#define KPF_COMPOUND_HEAD (1UL << 15) // since Linux 2.6.31
#define KPF_COMPOUND_TAIL (1UL << 16) // since Linux 2.6.31
#define KPF_HUGE          (1UL << 17) // since Linux 2.6.31
#define KPF_UNEVICTABLE   (1UL << 18) // since Linux 2.6.31
#define KPF_HWPOISON      (1UL << 19) // since Linux 2.6.31
#define KPF_NOPAGE        (1UL << 20) // since Linux 2.6.31
#define KPF_KSM           (1UL << 21) // since Linux 2.6.32
#define KPF_THP           (1UL << 22) // since Linux 3.4
#define KPF_BALLOON       (1UL << 23) // since Linux 3.18
#define KPF_ZERO_PAGE     (1UL << 24) // since Linux 4.0
#define KPF_IDLE          (1UL << 25) // since Linux 4.3
#define KPF_PGTABLE       (1UL << 26) // since Linux 4.18
#define KPF_FLAGS         ((KPF_PGTABLE << 1) - 1)

/*
 * Undocumented flags for "kernel hacking assistance". You should check the
 * running kernel source before using these. Available behind "hack" command
 * line argument.
 */
// include/linux/kernel-page-flags.h
#define KPF_RESERVED      (1UL << 32)
#define KPF_MLOCKED       (1UL << 33)
#define KPF_MAPPEDTODISK  (1UL << 34)
#define KPF_PRIVATE       (1UL << 35)
#define KPF_PRIVATE_2     (1UL << 36)
#define KPF_OWNER_PRIVATE (1UL << 37)
#define KPF_ARCH          (1UL << 38)
#define KPF_UNCACHED      (1UL << 39)
#define KPF_SOFTDIRTY     (1UL << 40)
#define KPF_ARCH_2        (1UL << 41)
#define KPF_HACK_FLAGS    (((KPF_ARCH_2 << 1) - 1) & ~(KPF_RESERVED - 1))

unsigned long read_ulong_at_offset(const char *path, off_t offset) {
	int fd;
	unsigned long res;
	ssize_t nread;

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Failed to open \"%s\": %s\n", path, strerror(errno));
		_exit(1);
	}

	nread = pread(fd, &res, sizeof(res), offset);

	if (nread != sizeof(res)) {
		if (nread == 0)
			fprintf(stderr, "EOF while reading \"%s\": page does not exist?\n", path);
		else
			perror("pread failed");

		_exit(1);
	}

	close(fd);
	return res;
}

unsigned long read_pagemap(const char *pid, unsigned long vaddr) {
	char path[128];
	sprintf(path, "/proc/%s/pagemap", pid);
	return read_ulong_at_offset(path, (vaddr >> PAGE_SHIFT) * 8);
}

void dump_pagemap(unsigned long pm, unsigned long vaddr) {
	unsigned long pfn;

	if (pm & PM_PRESENT) {
		pfn = pm & PM_PFRAME_MASK;
		printf("Paddr: 0x%lx, page: 0x%lx, PFN: 0x%lx\n",
			pfn << PAGE_SHIFT | (vaddr & ~PAGE_MASK), pfn << PAGE_SHIFT, pfn
		);
	} else if (pm & PM_SWAP) {
		printf("Swap type: 0x%lx, offset: 0x%lx\n",
			pm & PM_SWAP_TYPE_MASK,
			(pm & PM_SWAP_OFFSET_MASK) >> PM_SWAP_OFFSET_SHIFT
		);
	}

	printf("/proc/[pid]/pagemap: 0x%016lx =", pm);

	if (pm & PM_FLAGS) {
		if (pm & PM_PRESENT       ) fputs(" PRESENT"         , stdout);
		if (pm & PM_SWAP          ) fputs(" SWAP"            , stdout);
		if (pm & PM_FILE          ) fputs(" FILE(_OR_SHANON)", stdout);
		if (pm & PM_UFFD_WP       ) fputs(" UFFD_WP"         , stdout);
		if (pm & PM_MMAP_EXCLUSIVE) fputs(" MMAP_EXCLUSIVE"  , stdout);
		if (pm & PM_SOFT_DIRTY    ) fputs(" SOFT_DIRTY"      , stdout);
	} else {
		fputs(" no flags set, page does not exist?\n", stdout);
		_exit(1);
	}

	putchar('\n');
}

void dump_kpageflags_kpagecount(unsigned long pfn, bool hack, bool spacing) {
	unsigned long kpf   = read_ulong_at_offset("/proc/kpageflags", pfn * 8);
	unsigned long count = read_ulong_at_offset("/proc/kpagecount", pfn * 8);
	unsigned long mask  = KPF_FLAGS | (hack * KPF_HACK_FLAGS);

	printf("/proc/kpageflags%s: 0x%016lx =", spacing ? "   " : "", kpf);

	if (kpf & mask) {
		if (kpf & KPF_LOCKED       ) fputs(" LOCKED"       , stdout);
		if (kpf & KPF_ERROR        ) fputs(" ERROR"        , stdout);
		if (kpf & KPF_REFERENCED   ) fputs(" REFERENCED"   , stdout);
		if (kpf & KPF_UPTODATE     ) fputs(" UPTODATE"     , stdout);
		if (kpf & KPF_DIRTY        ) fputs(" DIRTY"        , stdout);
		if (kpf & KPF_LRU          ) fputs(" LRU"          , stdout);
		if (kpf & KPF_ACTIVE       ) fputs(" ACTIVE"       , stdout);
		if (kpf & KPF_SLAB         ) fputs(" SLAB"         , stdout);
		if (kpf & KPF_WRITEBACK    ) fputs(" WRITEBACK"    , stdout);
		if (kpf & KPF_RECLAIM      ) fputs(" RECLAIM"      , stdout);
		if (kpf & KPF_BUDDY        ) fputs(" BUDDY"        , stdout);
		if (kpf & KPF_MMAP         ) fputs(" MMAP"         , stdout);
		if (kpf & KPF_ANON         ) fputs(" ANON"         , stdout);
		if (kpf & KPF_SWAPCACHE    ) fputs(" SWAPCACHE"    , stdout);
		if (kpf & KPF_SWAPBACKED   ) fputs(" SWAPBACKED"   , stdout);
		if (kpf & KPF_COMPOUND_HEAD) fputs(" COMPOUND_HEAD", stdout);
		if (kpf & KPF_COMPOUND_TAIL) fputs(" COMPOUND_TAIL", stdout);
		if (kpf & KPF_HUGE         ) fputs(" HUGE"         , stdout);
		if (kpf & KPF_UNEVICTABLE  ) fputs(" UNEVICTABLE"  , stdout);
		if (kpf & KPF_HWPOISON     ) fputs(" HWPOISON"     , stdout);
		if (kpf & KPF_NOPAGE       ) fputs(" NOPAGE"       , stdout);
		if (kpf & KPF_KSM          ) fputs(" KSM"          , stdout);
		if (kpf & KPF_THP          ) fputs(" THP"          , stdout);
		if (kpf & KPF_BALLOON      ) fputs(" BALLOON"      , stdout);
		if (kpf & KPF_ZERO_PAGE    ) fputs(" ZERO_PAGE"    , stdout);
		if (kpf & KPF_IDLE         ) fputs(" IDLE"         , stdout);

		if (hack && (kpf & KPF_HACK_FLAGS)) {
			fputs((kpf & KPF_FLAGS) ? " | hack:" : " hack:", stdout);
			if (kpf & KPF_RESERVED     ) fputs(" RESERVED"     , stdout);
			if (kpf & KPF_MLOCKED      ) fputs(" MLOCKED"      , stdout);
			if (kpf & KPF_MAPPEDTODISK ) fputs(" MAPPEDTODISK" , stdout);
			if (kpf & KPF_PRIVATE      ) fputs(" PRIVATE"      , stdout);
			if (kpf & KPF_PRIVATE_2    ) fputs(" PRIVATE_2"    , stdout);
			if (kpf & KPF_OWNER_PRIVATE) fputs(" OWNER_PRIVATE", stdout);
			if (kpf & KPF_ARCH         ) fputs(" ARCH"         , stdout);
			if (kpf & KPF_UNCACHED     ) fputs(" UNCACHED"     , stdout);
			if (kpf & KPF_SOFTDIRTY    ) fputs(" SOFTDIRTY"    , stdout);
			if (kpf & KPF_ARCH_2       ) fputs(" ARCH_2"       , stdout);
		}
	} else {
		fputs(" no known flags set", stdout);
	}

	printf("\n/proc/kpagecount%s: %ld\n", spacing ? "   " : "", count);
}

void dump_page_info(const char *pid, unsigned long addr, bool hack) {
	unsigned long pm;

	printf("%caddr: 0x%lx, page: 0x%lx\n", pid ? 'V' : 'P', addr, addr & PAGE_MASK);

	if (pid) {
		// addr is virtual
		pm = read_pagemap(pid, addr);
		dump_pagemap(pm, addr);

		if (pm & PM_PRESENT)
			dump_kpageflags_kpagecount(pm & PM_PFRAME_MASK, hack, true);
	} else {
		// addr is physical
		dump_kpageflags_kpagecount(addr >> PAGE_SHIFT, hack, false);
	}
}

void usage_exit(const char *name) {
	fprintf(stderr, "Usage: %s PID VADDR [hack]\n", name);
	fprintf(stderr, "       %s self VADDR [hack]\n", name);
	fprintf(stderr, "       %s PADDR [hack]\n", name);
	exit(1);
}

int main(int argc, char **argv) {
	char *name = argv[0] ? argv[0] : "pageinfo";
	char *pid = NULL;
	bool hack = false;
	long tmp;
	char *endp;
	unsigned long addr;

	if (argc > 4)
		usage_exit(name);

	if (argc >= 3 && !strcmp(argv[argc - 1], "hack")) {
		hack = true;
		argc--;
	}

	if (argc != 2 && argc != 3)
		usage_exit(name);

	if (argc == 3) {
		pid = argv[1];

		if (strcmp(pid, "self")) {
			errno = 0;
			tmp = strtol(pid, &endp, 10);

			if (endp == pid || *endp != '\0' || errno == ERANGE || tmp < 1 || tmp > INT_MAX) {
				fputs("Invalid PID!\n", stderr);
				return 1;
			}
		}
	}

	errno = 0;
	addr = strtoul(argv[argc - 1], &endp, 0);

	if (endp == argv[argc - 1] || *endp != '\0' || errno == ERANGE) {
		fputs("Invalid address!\n", stderr);
		return 1;
	}

	dump_page_info(pid, addr, hack);
	return 0;
}
