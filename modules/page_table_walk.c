// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Walk the page table given a PID nad virtual address and find the physical
 * address, printing values/offsets of the entries for each page table level.
 * With dump=1 just dump the values of useful page table macros and exit.
 * Tested on kernel 5.10 x86_64.
 *
 * Usage: sudo insmod page_table_walk.ko pid=123 vaddr=0x1234
 *        sudo insmod page_table_walk.ko dump=1
 */

#include <linux/kernel.h>     // pr_info(), pr_*()
#include <linux/module.h>     // THIS_MODULE, MODULE_VERSION, ...
#include <linux/init.h>       // module_{init,exit}
#include <linux/pgtable.h>    // page table types/macros
#include <linux/sched/task.h> // struct task_struct, {get,put}_task_struct()

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

static void dump_pte_flags(pteval_t val)
{
	pteval_t mask = _PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED
			| _PAGE_DIRTY | _PAGE_SOFT_DIRTY | _PAGE_UFFD_WP
			| _PAGE_NX;

	pr_info("pte flags:");

	if (!(val & mask)) {
		pr_cont(" no interesting flags");
	} else {
		if (val & _PAGE_PRESENT   ) pr_cont(" PRESENT");
		if (val & _PAGE_RW        ) pr_cont(" RW");
		if (val & _PAGE_USER      ) pr_cont(" USER");
		if (val & _PAGE_ACCESSED  ) pr_cont(" ACCESSED");
		if (val & _PAGE_DIRTY     ) pr_cont(" DIRTY");
	#ifdef CONFIG_MEM_SOFT_DIRTY
		if (val & _PAGE_SOFT_DIRTY) pr_cont(" SOFT_DIRTY");
	#endif
		if (val & _PAGE_UFFD_WP   ) pr_cont(" UFFD_WP");
		if (val & _PAGE_NX        ) pr_cont(" NX");
	}

	pr_cont("\n");
}

static void walk(struct task_struct *task, unsigned long vaddr)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = pgd_offset(task->mm, vaddr);
	pr_info("pgd: idx 0x%-3lx val 0x%lx\n", pgd_index(vaddr), pgd_val(*pgd));
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		pr_info("pgd is %s\n", pgd_none(*pgd) ? "none" : "bad");
		return;
	}

	p4d = p4d_offset(pgd, vaddr);
	pr_info("p4d: idx 0x%-3lx val 0x%lx\n", p4d_index(vaddr), p4d_val(*p4d));
	if (p4d_none(*p4d) || p4d_bad(*p4d)) {
		pr_info("p4d is %s\n", p4d_none(*p4d) ? "none" : "bad");
		return;
	}

	pud = pud_offset(p4d, vaddr);
	pr_info("pud: idx 0x%-3lx val 0x%lx\n", pud_index(vaddr), pud_val(*pud));
	if (pud_none(*pud) || pud_bad(*pud)) {
		pr_info("pud is %s\n", pud_none(*pud) ? "none" : "bad");
		return;
	}

	pmd = pmd_offset(pud, vaddr);
	pr_info("pmd: idx 0x%-3lx val 0x%lx\n", pmd_index(vaddr), pmd_val(*pmd));
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		pr_info("pmd is %s\n", pmd_none(*pmd) ? "none" : "bad");
		return;
	}

	pte = pte_offset_kernel(pmd, vaddr);
	pr_info("pte: idx 0x%-3lx val 0x%lx\n", pte_index(vaddr), pte_val(*pte));
	if (pte_none(*pte)) {
		pr_info("pte is none\n");
		return;
	}

	dump_pte_flags(pte_val(*pte));

	pr_info("paddr: 0x%lx\n",
		(pte_pfn(*pte) << PAGE_SHIFT) | (vaddr & ~PAGE_MASK));
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
	return -123;
}

module_init(page_table_walk_init);
MODULE_VERSION("0.2");
MODULE_DESCRIPTION("Walk the page table and dump entries given a PID and a"
		   "virtual address");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
