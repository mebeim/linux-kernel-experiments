// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Walk user/kernel page tables given a virtual address (plus PID for user page
 * tables) and find the physical address, printing values/offsets/flags of the
 * entries for each page table level. With dump=1 just dump the values of useful
 * page table macros and exit. This module was written for x86_64. The
 * correspondence between page table types and Intel doc is: pgd=PML5E,
 * p4d=PML4E, pud=PDPTE, pmd=PDE, pte=PTE.
 *
 * Tested on kernel 5.10, 5.17.
 *
 * Usage: sudo insmod page_table_walk.ko pid=123 vaddr=0x1234  # user
 *        sudo insmod page_table_walk.ko pid=0 vaddr=0x1234    # kernel
 *        sudo insmod page_table_walk.ko dump=1
 *
 * Changelog:
 *
 * v0.9: Correctly handle 1G huge pages rewriting bogus pud_ helpers. Correctly
 *       handle PROTNONE at all levels. Invert PROTNONE entries when needed.
 *       Dump more interesting macros (PHYSICAL_PAGE_*_MASK). Thanks to the
 *       people over at #mm on irc.oftc.net (linux-mm.org) for clarifications.
 * v0.8: Detect swapped pages and dump swap entries, use {pud,pmd}_large()
 *       instead of _huge() to detect huge pages.
 * v0.7: Print correct paddr for huge pages, detect PROTNONE pages.
 * v0.6: Appropriately use mm{get,put}() and task_[un]lock() to get ahold of
 *       task->mm or task->active_mm, put_task_struct() in case of failure to
 *       get task mm/active_mm.
 * v0.5: Support walking kernel page tables.
 * v0.4: Support PAT bit for huge pages, support kthreads, use ulong for vaddr,
 *       fix checks for present page table entries.
 * v0.3: Detect zero page, support huge pages.
 * v0.2: Generalize/refactor code.
 * v0.1: Initial version.
 */

#include <linux/kernel.h>        // pr_info(), pr_*()
#include <linux/module.h>        // THIS_MODULE, MODULE_VERSION, ...
#include <linux/init.h>          // module_{init,exit}
#include <linux/pgtable.h>       // page table types/macros, ZERO_PAGE macro
#include <linux/sched/task.h>    // struct task_struct, {get,put}_task_struct()
#include <linux/sched/mm.h>      // mmget(), mmput()
#include <linux/swap.h>          // needed by linux/swapops.h
#include <linux/swapops.h>       // swp_{type,offset}(), p{md,te}_to_swp_entry()
#include <asm/msr-index.h>       // MSR defines
#include <asm/msr.h>             // r/w MSR funcs/macros
#include <asm/special_insns.h>   // r/w control regs
#include <asm/processor-flags.h> // control regs flags
#include <asm/io.h>              // phys_to_virt()
#include <asm/pgtable-invert.h>  // __pte_needs_invert()

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int user_pid = -1;
module_param_named(pid, user_pid, int, 0);
MODULE_PARM_DESC(pid, "User PID of the process to inspect (-1 for current, 0 for kernel)");

static unsigned long vaddr;
module_param_named(vaddr, vaddr, ulong, 0);
MODULE_PARM_DESC(vaddr, "Virtual address to use for page table walk");

static bool dump;
module_param_named(dump, dump, bool, 0);
MODULE_PARM_DESC(dump, "Just dump page table related macros and exit");

/**
 * Find task_struct given **userspace** PID.
 *
 * NOTE: caller must put_task_struct() when done.
 */
static struct task_struct *get_user_pid_task(pid_t pid) {
	return get_pid_task(find_get_pid(pid), PIDTYPE_PID);
}

static inline int rdmsrl_wrap(const char *name, int msrno,
			      unsigned long long *pval)
{
	int err;

	if ((err = rdmsrl_safe(msrno, pval)))
		pr_err("rdmsrl_safe(%s) failed, aborting.\n", name);

	return err;
}
#define RDMSR(msr, val) rdmsrl_wrap(#msr, msr, &(val))

static void dump_macros(void)
{
	pr_info("PGDIR_SHIFT            = %d\n", PGDIR_SHIFT);
	pr_info("P4D_SHIFT              = %d\n", P4D_SHIFT);
	pr_info("PUD_SHIFT              = %d\n", PUD_SHIFT);
	pr_info("PMD_SHIFT              = %d\n", PMD_SHIFT);
	pr_info("PAGE_SHIFT             = %d\n", PAGE_SHIFT);
	pr_info("PTRS_PER_PGD           = %d\n", PTRS_PER_PGD);
	pr_info("PTRS_PER_P4D           = %d\n", PTRS_PER_P4D);
	pr_info("PTRS_PER_PUD           = %d\n", PTRS_PER_PUD);
	pr_info("PTRS_PER_PMD           = %d\n", PTRS_PER_PMD);
	pr_info("PTRS_PER_PTE           = %d\n", PTRS_PER_PTE);
	pr_info("PGDIR_MASK             = 0x%016lx\n", PGDIR_MASK);
	pr_info("P4D_MASK               = 0x%016lx\n", P4D_MASK);
	pr_info("PUD_MASK               = 0x%016lx\n", PUD_MASK);
	pr_info("PMD_MASK               = 0x%016lx\n", PMD_MASK);
	pr_info("PAGE_MASK              = 0x%016lx\n", PAGE_MASK);
	pr_info("PMD_PAGE_MASK          = 0x%016lx\n", PMD_PAGE_MASK);
	pr_info("PUD_PAGE_MASK          = 0x%016lx\n", PUD_PAGE_MASK);
	pr_info("PHYSICAL_PAGE_MASK     = 0x%016lx\n", (unsigned long)PHYSICAL_PAGE_MASK);
	pr_info("PHYSICAL_PMD_PAGE_MASK = 0x%016lx\n", (unsigned long)PHYSICAL_PMD_PAGE_MASK);
	pr_info("PHYSICAL_PUD_PAGE_MASK = 0x%016lx\n", (unsigned long)PHYSICAL_PUD_PAGE_MASK);
	pr_info("PTE_PFN_MASK           = 0x%016lx\n", PTE_PFN_MASK);
	pr_info("PAGE_OFFSET            = 0x%016lx\n", PAGE_OFFSET);
}

/* Fix some pud-related helpers to behave correctly with 1G huge pages */

#define pud_present pud_present_good
static inline int pud_present_good(pud_t pud)
{
	/*
	 * NOTE: this might need change if 1G THPs become available because
	 * split_huge_page temporarily clears the present bit, but the _PAGE_PSE
	 * bit remains set at all times while the _PAGE_PRESENT bit is clear.
	 * So in such case the check should become:
	 *
	 *     pud_flags(pud) & (_PAGE_PRESENT | _PAGE_PROTNONE | _PAGE_PSE)
	 *
	 * See comment above pmd_present() at arch/x86/include/asm/pgtable.h.
	 */
	return pud_flags(pud) & (_PAGE_PRESENT | _PAGE_PROTNONE);
}

#define pud_large pud_large_good
static inline int pud_large_good(pud_t pud)
{
	return pud_flags(pud) & _PAGE_PSE;
}

/* Helpers for easy paddr calculation */

static inline unsigned long pud_paddr(pud_t pud, unsigned long vaddr)
{
	return (pud_pfn(pud) << PAGE_SHIFT) | (vaddr & ~PUD_PAGE_MASK);
}

static inline unsigned long pmd_paddr(pmd_t pmd, unsigned long vaddr)
{
	return (pmd_pfn(pmd) << PAGE_SHIFT) | (vaddr & ~PMD_PAGE_MASK);
}

static inline unsigned long pte_paddr(pte_t pte, unsigned long vaddr)
{
	return (pte_pfn(pte) << PAGE_SHIFT) | (vaddr & ~PAGE_MASK);
}

static inline bool is_zero_page_pte(pte_t pte)
{
	return pte_pfn(pte) == page_to_pfn(ZERO_PAGE(0));
}

/**
 * The PFN for PROTNONE entries is inverted to stop speculation (L1TF
 * mitigation). If we want the actual {pte,pmd,pud}_val() we need to invert when
 * needed. See arch/x86/include/asm/pgtable-invert.h.
 */
static inline u64 invert_val_if_needed(u64 val)
{
	/*
	 * Actually, a bit more than the PFN is inverted, don't know exactly
	 * why. The inversion seems to be done with PHYSICAL_PAGE_MASK
	 * regardless of level.
	 */
	const u64 mask = PHYSICAL_PAGE_MASK;
	return __pte_needs_invert(val) ? ((val & ~mask) | (~val & mask)) : val;
}

static void dump_flags_common(unsigned long val)
{
	if (val & _PAGE_PRESENT ) pr_cont(" PRESENT");
	if (val & _PAGE_RW      ) pr_cont(" RW");
	if (val & _PAGE_USER    ) pr_cont(" USER");
	else                      pr_cont(" KERNEL");
	if (val & _PAGE_PWT     ) pr_cont(" PWT");
	if (val & _PAGE_PCD     ) pr_cont(" PCD");
	if (val & _PAGE_ACCESSED) pr_cont(" ACCESSED");
}

static void dump_flags_last_level(unsigned long val, bool pke)
{
	/*
	 * Pages with no permissions have the PRESENT bit clear and the PROTNONE
	 * bit set. PROTNONE and GLOBAL are the same bit. The check for PROTNONE
	 * is ((val & (_PAGE_PRESENT|_PAGE_PROTNONE)) == _PAGE_PROTNONE) and
	 * should be the same for leaf entries at all levels (pte, pmd, pud).
	 */
	static_assert(_PAGE_GLOBAL == _PAGE_PROTNONE);

	if (val & _PAGE_DIRTY          ) pr_cont(" DIRTY");
	if (val & _PAGE_PROTNONE) {
		if (val & _PAGE_PRESENT) pr_cont(" GLOBAL");
		else                     pr_cont(" PROTNONE");
	}
#ifdef CONFIG_HAVE_ARCH_USERFAULTFD_WP
	if (val & _PAGE_UFFD_WP        ) pr_cont(" UFFD_WP");
#endif
#ifdef CONFIG_MEM_SOFT_DIRTY
	if (val & _PAGE_SOFT_DIRTY     ) pr_cont(" SOFT_DIRTY");
#endif
	if (val & _PAGE_NX             ) pr_cont(" NX");

	if (pke)
		pr_cont(" PKEY=%lx",
			(val & _PAGE_PKEY_MASK) >> _PAGE_BIT_PKEY_BIT0);
}

// See comments in arch/x86/include/asm/pgtable_64.h
static void dump_swap_flags(unsigned long val)
{
	if (val & _PAGE_PROTNONE      ) pr_cont(" PROTNONE");
#ifdef CONFIG_HAVE_ARCH_USERFAULTFD_WP
	if (val & _PAGE_SWP_UFFD_WP   ) pr_cont(" UFFD_WP");
#endif
#ifdef CONFIG_MEM_SOFT_DIRTY
	if (val & _PAGE_SWP_SOFT_DIRTY) pr_cont(" SOFT_DIRTY");
#endif
}

static void dump_swap_entry(swp_entry_t entry)
{
	static_assert(sizeof(pgoff_t) == sizeof(unsigned long));
	pr_info("Swap: type %x offset %lx", swp_type(entry), swp_offset(entry));
}

static void dump_paddr(unsigned long paddr, bool is_zero)
{
	pr_info("paddr: 0x%lx%s\n", paddr, is_zero ? " (zero page)" : "");
}

static bool dump_pgd(pgd_t pgd, unsigned long vaddr)
{
	pgdval_t val = pgd_val(pgd);

	pr_info("pgd: idx %03lx val %016lx", pgd_index(vaddr), val);

	if (!pgd_present(pgd)) {
		pr_info("pgd not present\n");
		return true;
	}

	dump_flags_common((unsigned long)val);
	pr_cont("\n");

	return false;
}

static bool dump_p4d(p4d_t p4d, unsigned long vaddr)
{
	p4dval_t val = p4d_val(p4d);

	pr_info("p4d: idx %03lx val %016lx", p4d_index(vaddr), val);

	if (!p4d_present(p4d)) {
		pr_info("p4d not present\n");
		return true;
	}

	dump_flags_common((unsigned long)val);
	pr_cont("\n");

	return false;
}

static bool dump_pud(pud_t pud, unsigned long vaddr, bool pke)
{
	pudval_t val = invert_val_if_needed(pud_val(pud));

	pr_info("pud: idx %03lx val %016lx", pud_index(vaddr), val);

	if (!pud_present(pud)) {
		pr_cont(" not present\n");
		return true;
	}

	dump_flags_common((unsigned long)val);

	if (pud_large(pud)) {
		pr_cont(" 1G");
		if (val & _PAGE_PAT_LARGE)
			pr_cont(" PAT");

		dump_flags_last_level((unsigned long)val, pke);
		pr_cont("\n");
		dump_paddr(pud_paddr(pud, vaddr), false);
		return true;
	}

	pr_cont("\n");
	return false;
}

static bool dump_pmd(pmd_t pmd, unsigned long vaddr, bool pke)
{
	pmdval_t val = invert_val_if_needed(pmd_val(pmd));

	pr_info("pmd: idx %03lx val %016lx", pmd_index(vaddr), val);

	if (pmd_none(pmd)) {
		pr_cont(" none\n");
		return true;
	}

	// is_swap_pmd(pmd) <==> !pmd_none(pmd) && !pmd_present(pmd)
	if (!pmd_present(pmd)) {
#if defined(CONFIG_TRANSPARENT_HUGEPAGE) && defined(CONFIG_ARCH_ENABLE_THP_MIGRATION)
		// Only *transparent* huge pages can be swapped out.
		dump_swap_flags((unsigned long)val);
		pr_cont("\n");
		dump_swap_entry(pmd_to_swp_entry(pmd));
#else
		pr_cont(" not present\n");
#endif
		return true;
	}

	dump_flags_common((unsigned long)val);

	/*
	 * pmd_huge() "returns 1 if @pmd is hugetlb related entry, that is
	 * normal hugetlb entry or non-present (migration or hwpoisoned) hugetlb
	 * entry" (where I suppose "hugetlb entry" means MAP_HUGETLB)... so we
	 * want pmd_large() here.
	 */
	if (pmd_large(pmd)) {
		pr_cont(" 2M");
		if (val & _PAGE_PAT_LARGE)
			pr_cont(" PAT");

		dump_flags_last_level((unsigned long)val, pke);
		pr_cont("\n");

		/*
		 * Unfortunately huge_zero_page (mm/huge_memory.c) is not
		 * exported, so there's no decent way to detect huge zero pages,
		 * though /proc/kpageflags has this info.
		 *
		 * Note for future: if detection becomes possible, make sure to
		 * appropriately wrap it in #ifdef CONFIG_TRANSPARENT_HUGEPAGE.
		 */
		dump_paddr(pmd_paddr(pmd, vaddr), false);
		return true;
	}

	pr_cont("\n");
	return false;
}

static void dump_pte(pte_t pte, unsigned long vaddr, bool pke)
{
	pteval_t val = invert_val_if_needed(pte_val(pte));

	pr_info("pte: idx %03lx val %016lx", pte_index(vaddr), val);

	if (pte_none(pte)) {
		pr_cont(" none\n");
		return;
	}

	// is_swap_pte(pte) <==> !pte_none(pte) && !pte_present(pte)
	if (!pte_present(pte)) {
		dump_swap_flags((unsigned long)val);
		pr_cont("\n");
		dump_swap_entry(pte_to_swp_entry(pte));
		return;
	}

	dump_flags_common((unsigned long)val);

	if (val & _PAGE_PAT)
		pr_cont(" PAT");

	dump_flags_last_level((unsigned long)val, pke);
	pr_cont("\n");
	dump_paddr(pte_paddr(pte, vaddr), is_zero_page_pte(pte));
}

static void walk_4l(pgd_t *pgdp, unsigned long vaddr, bool pke, p4d_t *p4dp)
{
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;

	if (!p4dp) {
		// We are doing a pure 4-level walk, start from pgd
		if (dump_pgd(*pgdp, vaddr))
			return;

		p4dp = p4d_offset(pgdp, vaddr);
		// Do not dump p4d since p4d == pgd in this case
	}

	pudp = pud_offset(p4dp, vaddr);
	if (dump_pud(*pudp, vaddr, pke))
		return;

	pmdp = pmd_offset(pudp, vaddr);
	if (dump_pmd(*pmdp, vaddr, pke))
		return;

	ptep = pte_offset_kernel(pmdp, vaddr);
	dump_pte(*ptep, vaddr, pke);
}

static void walk_5l(pgd_t *pgdp, unsigned long vaddr, bool pke)
{
	p4d_t *p4dp;

	if (dump_pgd(*pgdp, vaddr))
		return;

	p4dp = p4d_offset(pgdp, vaddr);
	if (dump_p4d(*p4dp, vaddr))
		return;

	walk_4l(pgdp, vaddr, pke, p4dp);
}

static int walk(pgd_t *pgdp, unsigned long vaddr)
{
	unsigned long long efer;
	unsigned long cr4;
	bool pke = false;
	int err;

	/*
	 * Not sure how much sense it makes to do all these checks. Some are
	 * redundant as this module wouldn't even compile or be inserted.
	 */

	if ((err = RDMSR(MSR_EFER, efer)))
		return err;

	if (!(read_cr0() & X86_CR0_PG)) {
		pr_err("Paging disabled, aborting.\n");
		return 0;
	}

	if ((efer & (EFER_LME|EFER_LMA)) != (EFER_LME|EFER_LMA)) {
		pr_err("Not in IA-32e mode, aborting.\n");
		return 0;
	}

	cr4 = __read_cr4();
	if (!(cr4 & X86_CR4_PAE)) {
		pr_err("PAE disabled, aborting.\n");
		return 0;
	}

#ifdef CONFIG_X86_INTEL_MEMORY_PROTECTION_KEYS
	pke = !!(cr4 & X86_CR4_PKE);
#endif

	if (cr4 & X86_CR4_LA57)
		walk_5l(pgdp, vaddr, pke);
	else
		walk_4l(pgdp, vaddr, pke, NULL);

	return 0;
}

static int walk_kernel(unsigned long vaddr) {
	pgd_t *pgdp;

	pr_info("Examining kernel vaddr 0x%lx\n", vaddr);

	/*
	 * In theory we would just use init_mm.pgd here, however init_mm is not
	 * exported for us to use, so read cr3 manually and convert PA to VA.
	 */
	pgdp = phys_to_virt(__read_cr3() & ~0xfff);
	return walk(pgd_offset_pgd(pgdp, vaddr), vaddr);
}

static int walk_user(int user_pid, unsigned long vaddr) {
	char comm[TASK_COMM_LEN];
	struct task_struct *task;
	struct mm_struct *mm;
	int res;

	if (user_pid == -1) {
		task = current;
		get_task_struct(task);
	} else {
		task = get_user_pid_task(user_pid);
		if (task == NULL) {
			pr_err("No task with user PID = %d.\n", user_pid);
			return -ESRCH;
		}
	}

	pr_info("Examining %s[%d] vaddr 0x%lx\n", get_task_comm(comm, task),
		task->pid, vaddr);

	/*
	 * Can't use get_task_mm() here if we also want to handle kthreads,
	 * which don't have their own ->mm.
	 */
	task_lock(task);

	if (!(mm = task->mm)) {
		if (!(mm = task->active_mm)) {
			/*
			 * This will happen if we try to inspect page tables of
			 * kthreads since those do not have their own mm;
			 * instead they have an active_mm stolen from some other
			 * task, but only if they are *currently running* (good
			 * luck trying to catch those). Indeed it does not make
			 * much sense to inspect kthread page tables; just
			 * inspect kernel page tables passing pid=0 instead.
			 */
			pr_err("Task has no own mm nor active mm, aborting.\n");
			task_unlock(task);
			put_task_struct(task);
			return -ESRCH;
		}

		pr_warn("Task does not have own mm, using active_mm.\n");
	}

	mmget(mm);
	task_unlock(task);
	put_task_struct(task);

	res = walk(pgd_offset(mm, vaddr), vaddr);
	mmput(mm);
	return res;
}

static int __init page_table_walk_init(void)
{
	int err;

	if (dump) {
		dump_macros();
	} else {
		if (user_pid)
			err = walk_user(user_pid, vaddr);
		else
			err = walk_kernel(vaddr);

		if (err)
			return err;
	}

	/*
	 * Just fail loading with a random error to make it simpler to use this
	 * module multiple times in a row.
	 */
	return -ECANCELED;
}

module_init(page_table_walk_init);
MODULE_VERSION("0.9");
MODULE_DESCRIPTION("Walk user/kernel page tables given a virtual address (plus"
		   "PID for user page tables) and dump entries and flags");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
