// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Walk the page table given a PID and virtual address and find the physical
 * address, printing values/offsets/flags of the entries for each page table
 * level. With dump=1 just dump the values of useful page table macros and exit.
 * This module was written for x86_64. The correspondence between page table
 * types and Intel doc is: pgd=PML5E, p4d=PML4E, pud=PDPTE, pmd=PDE, pte=PTE.
 * Tested on kernel 5.10 x86_64.
 *
 * Usage: sudo insmod page_table_walk.ko pid=123 vaddr=0x1234
 *        sudo insmod page_table_walk.ko dump=1
 */

#include <linux/kernel.h>        // pr_info(), pr_*()
#include <linux/module.h>        // THIS_MODULE, MODULE_VERSION, ...
#include <linux/init.h>          // module_{init,exit}
#include <linux/pgtable.h>       // page table types/macros
#include <linux/sched/task.h>    // struct task_struct, {get,put}_task_struct()
#include <asm/msr-index.h>       // MSR defines
#include <asm/msr.h>             // r/w MSR funcs/macros
#include <asm/special_insns.h>   // r/w control regs
#include <asm/processor-flags.h> // control regs flags

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int user_pid = -1;
module_param_named(pid, user_pid, int, 0);
MODULE_PARM_DESC(pid, "User PID of the process to inspect (-1 for current)");

static long vaddr;
module_param_named(vaddr, vaddr, long, 0);
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
	pr_info("PGDIR_SHIFT  = %d\n", PGDIR_SHIFT);
	pr_info("P4D_SHIFT    = %d\n", P4D_SHIFT);
	pr_info("PUD_SHIFT    = %d\n", PUD_SHIFT);
	pr_info("PMD_SHIFT    = %d\n", PMD_SHIFT);
	pr_info("PAGE_SHIFT   = %d\n", PAGE_SHIFT);
	pr_info("PTRS_PER_PGD = %d\n", PTRS_PER_PGD);
	pr_info("PTRS_PER_P4D = %d\n", PTRS_PER_P4D);
	pr_info("PTRS_PER_PUD = %d\n", PTRS_PER_PUD);
	pr_info("PTRS_PER_PMD = %d\n", PTRS_PER_PMD);
	pr_info("PTRS_PER_PTE = %d\n", PTRS_PER_PTE);
	pr_info("PGDIR_MASK   = 0x%016lx\n", PGDIR_MASK);
	pr_info("P4D_MASK     = 0x%016lx\n", P4D_MASK);
	pr_info("PUD_MASK     = 0x%016lx\n", PUD_MASK);
	pr_info("PMD_MASK     = 0x%016lx\n", PMD_MASK);
	pr_info("PAGE_MASK    = 0x%016lx\n", PAGE_MASK);
	pr_info("PTE_PFN_MASK = 0x%016lx\n", PTE_PFN_MASK);
	pr_info("PAGE_OFFSET  = 0x%016lx\n", PAGE_OFFSET);
}

// Borrowed from arch/x86/mm/hugetlbpage.c
static int pmd_huge(pmd_t pmd)
{
	return !pmd_none(pmd) &&
		(pmd_val(pmd) & (_PAGE_PRESENT|_PAGE_PSE)) != _PAGE_PRESENT;
}

static int pud_huge(pud_t pud)
{
	return !!(pud_val(pud) & _PAGE_PSE);
}

static void dump_page_flags_common(unsigned long val)
{
	if (val & _PAGE_PRESENT ) pr_cont(" PRESENT");
	if (val & _PAGE_RW      ) pr_cont(" RW");
	if (val & _PAGE_USER    ) pr_cont(" USER");
	else                      pr_cont(" KERNEL");
	if (val & _PAGE_PWT     ) pr_cont(" PWT");
	if (val & _PAGE_PCD     ) pr_cont(" PCD");
	if (val & _PAGE_ACCESSED) pr_cont(" ACCESSED");
}

static void dump_page_flags_last_level(unsigned long val, bool pke)
{
	if (val & _PAGE_DIRTY     ) pr_cont(" DIRTY");
	if (val & _PAGE_GLOBAL    ) pr_cont(" GLOBAL");
#ifdef CONFIG_HAVE_ARCH_USERFAULTFD_WP
	if (val & _PAGE_UFFD_WP   ) pr_cont(" UFFD_WP");
#endif
#ifdef CONFIG_MEM_SOFT_DIRTY
	if (val & _PAGE_SOFT_DIRTY) pr_cont(" SOFT_DIRTY");
#endif
	if (val & _PAGE_NX        ) pr_cont(" NX");

	if (pke)
		pr_cont(" PKEY=%lx",
			(val & _PAGE_PKEY_MASK) >> _PAGE_BIT_PKEY_BIT0);
}

static bool dump_pgd(pgd_t pgd, unsigned long vaddr)
{
	pgdval_t val = pgd_val(pgd);

	if (pgd_none(pgd) || pgd_bad(pgd)) {
		pr_info("pgd is %s\n", pgd_none(pgd) ? "none" : "bad");
		return false;
	}

	pr_info("pgd: idx %03lx val %016lx", pgd_index(vaddr), val);
	dump_page_flags_common((unsigned long)val);
	pr_cont("\n");

	return true;
}

static bool dump_p4d(p4d_t p4d, unsigned long vaddr)
{
	p4dval_t val = p4d_val(p4d);

	if (p4d_none(p4d) || p4d_bad(p4d)) {
		pr_info("p4d is %s\n", p4d_none(p4d) ? "none" : "bad");
		return false;
	}

	pr_info("p4d: idx %03lx val %016lx", p4d_index(vaddr), val);
	dump_page_flags_common((unsigned long)val);
	pr_cont("\n");

	return true;
}

static bool dump_pud(pud_t pud, unsigned long vaddr, bool pke)
{
	pudval_t val = pud_val(pud);

	if (pud_none(pud) || (pud_bad(pud) && !pud_huge(pud))) {
		pr_info("pud is %s\n", pud_none(pud) ? "none" : "bad");
		return false;
	}

	pr_info("pud: idx %03lx val %016lx", pud_index(vaddr), val);
	dump_page_flags_common((unsigned long)val);

	if (pud_huge(pud)) {
		pr_cont(" 1G");
		dump_page_flags_last_level((unsigned long)val, pke);
		pr_cont("\n");

		pr_info("paddr: 0x%lx\n",
			(pud_pfn(pud) << PAGE_SHIFT) | (vaddr & ~PAGE_MASK));

		return false;
	}

	pr_cont("\n");
	return true;
}

static bool dump_pmd(pmd_t pmd, unsigned long vaddr, bool pke)
{
	pmdval_t val = pmd_val(pmd);

	if (pmd_none(pmd) || (pmd_bad(pmd) && !pmd_huge(pmd))) {
		pr_info("pmd is %s\n", pmd_none(pmd) ? "none" : "bad");
		return false;
	}

	pr_info("pmd: idx %03lx val %016lx", pmd_index(vaddr), val);
	dump_page_flags_common((unsigned long)val);

	if (pmd_huge(pmd)) {
		pr_cont(" 2M");
		dump_page_flags_last_level((unsigned long)val, pke);
		pr_cont("\n");

		pr_info("paddr: 0x%lx\n",
			(pmd_pfn(pmd) << PAGE_SHIFT) | (vaddr & ~PAGE_MASK));

		return false;
	}

	pr_cont("\n");
	return true;
}

static void dump_pte(pte_t pte, unsigned long vaddr, bool pke)
{
	pteval_t val = pte_val(pte);

	if (pte_none(pte)) {
		pr_info("pte is none\n");
		return;
	}

	pr_info("pte: idx %03lx val %016lx", pte_index(vaddr), val);
	dump_page_flags_common((unsigned long)val);

	if (val & _PAGE_PAT)
		pr_cont(" PAT");

	dump_page_flags_last_level((unsigned long)val, pke);
	pr_cont("\n");

	pr_info("paddr: 0x%lx\n",
		(pte_pfn(pte) << PAGE_SHIFT) | (vaddr & ~PAGE_MASK));
}

static void walk_4l(const struct task_struct *task, unsigned long vaddr,
		    bool pke, p4d_t *p4d)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	if (!p4d) {
		// We are doing a pure 4-level walk, start from pgd
		pgd = pgd_offset(task->mm, vaddr);
		if (!dump_pgd(*pgd, vaddr))
			return;

		p4d = p4d_offset(pgd, vaddr);
		// Do not dump p4d since p4d == pgd in this case
	}

	pud = pud_offset(p4d, vaddr);
	if (!dump_pud(*pud, vaddr, pke))
		return;

	pmd = pmd_offset(pud, vaddr);
	if (!dump_pmd(*pmd, vaddr, pke))
		return;

	pte = pte_offset_kernel(pmd, vaddr);
	dump_pte(*pte, vaddr, pke);
}

static void walk_5l(const struct task_struct *task, unsigned long vaddr,
		    bool pke)
{
	pgd_t *pgd;
	p4d_t *p4d;

	pgd = pgd_offset(task->mm, vaddr);
	if (!dump_pgd(*pgd, vaddr))
		return;

	p4d = p4d_offset(pgd, vaddr);
	if (!dump_p4d(*p4d, vaddr))
		return;

	walk_4l(task, vaddr, pke, p4d);
}

void walk(const struct task_struct *task, unsigned long vaddr)
{
	unsigned long long efer;
	unsigned long cr4;
	bool pke = false;

	if (RDMSR(MSR_EFER, efer))
		return;

	if ((efer & (EFER_LME|EFER_LMA)) != (EFER_LME|EFER_LMA)) {
		pr_err("Not in IA-32e mode, aborting.\n");
		return;
	}

	if (!(read_cr0() & X86_CR0_PG)) {
		pr_err("Paging disabled, aborting.\n");
		return;
	}

	cr4 = __read_cr4();
	if (!(cr4 & X86_CR4_PAE)) {
		pr_err("PAE disabled, aborting.\n");
		return;
	}

#ifdef CONFIG_X86_INTEL_MEMORY_PROTECTION_KEYS
	pke = !!(cr4 & X86_CR4_PKE);
#endif

	if (cr4 & X86_CR4_LA57)
		walk_5l(task, vaddr, pke);
	else
		walk_4l(task, vaddr, pke, NULL);
}

static int __init page_table_walk_init(void)
{
	struct task_struct *task;

	if (dump) {
		dump_macros();
		goto out;
	}

	if (user_pid == -1) {
		task = current;
		get_task_struct(task);
	} else {
		task = get_user_pid_task(user_pid);
		if (task == NULL) {
			pr_err("No process with user PID = %d.\n", user_pid);
			return -ESRCH; // No such process.
		}
	}

	pr_info("Examining %s[%d] vaddr 0x%lx\n", task->comm, task->pid, vaddr);
	walk(task, vaddr);
	put_task_struct(task);

out:
	// Just fail loading with a random error to make it simpler to use this
	// module multiple times in a row.
	return -ECANCELED;
}

module_init(page_table_walk_init);
MODULE_VERSION("0.3");
MODULE_DESCRIPTION("Walk the page table and dump entries and flags given a PID "
		   "and a virtual address");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
