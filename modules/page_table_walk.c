// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Walk the page table given a virtual address and find the physical address,
 * printing values/offsets of the entries for each page table level. With dump=1
 * this module also dumps the values of useful page table macros.
 * Tested on kernel 5.10 x86_64.
 * Usage: sudo insmod page_table_walk.ko vaddr=0x1234 [dump=1]
 */

#include <linux/kernel.h>  // pr_info(), pr_*()
#include <linux/module.h>  // THIS_MODULE, MODULE_VERSION, ...
#include <linux/init.h>    // module_{init,exit}
#include <linux/pgtable.h> // page table types/macros

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static long vaddr;
module_param_named(vaddr, vaddr, long, 0);
MODULE_PARM_DESC(vaddr, "Virtual address to use for page table walk");

static bool dump;
module_param_named(dump, dump, bool, 0);
MODULE_PARM_DESC(dump, "Dump page table related macros");

static void dump_macros(void)
{
	pr_info("PAGE_OFFSET  = 0x%lx\n", PAGE_OFFSET);
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
	pr_info("PAGE_MASK    = 0x%lx\n", PAGE_MASK);
}

static void vaddr2paddr(unsigned long vaddr)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pr_info("vaddr     = 0x%lx\n", vaddr);

	pgd = pgd_offset(current->mm, vaddr);
	pr_info("pgd_val   = 0x%lx\n", pgd_val(*pgd));
	pr_info("pgd_index = 0x%lx\n", pgd_index(vaddr));
	if (pgd_none(*pgd)) {
		pr_info("pgd is none\n");
		return;
	}

	p4d = p4d_offset(pgd, vaddr);
	pr_info("p4d_val   = 0x%lx\n", p4d_val(*p4d));
	pr_info("p4d_index = 0x%lx\n", p4d_index(vaddr));
	if (p4d_none(*p4d)) {
		pr_info("p4d is none\n");
		return;
	}

	pud = pud_offset(p4d, vaddr);
	pr_info("pud_val   = 0x%lx\n", pud_val(*pud));
	pr_info("pud_index = 0x%lx\n", pud_index(vaddr));
	if (pud_none(*pud)) {
		pr_info("pud is none\n");
		return;
	}

	pmd = pmd_offset(pud, vaddr);
	pr_info("pmd_val   = 0x%lx\n", pmd_val(*pmd));
	pr_info("pmd_index = 0x%lx\n", pmd_index(vaddr));
	if (pmd_none(*pmd)) {
		pr_info("pmd is none\n");
		return;
	}

	pte = pte_offset_kernel(pmd, vaddr);
	pr_info("pte_val   = 0x%lx\n", pte_val(*pte));
	pr_info("pte_index = 0x%lx\n", pte_index(vaddr));
	if (pte_none(*pte)) {
		pr_info("pte is none\n");
		return;
	}

	pr_info("paddr     = 0x%lx\n",
		(pte_val(*pte) & PAGE_MASK) | (vaddr & ~PAGE_MASK));
}

static int __init page_table_walk_init(void)
{
	if (dump)
		dump_macros();

	vaddr2paddr(vaddr);

	// Just fail loading with a random error to make it simpler to use this
	// module multiple times in a row.
	return -123;
}

module_init(page_table_walk_init);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Walk the page table and dump entries given a virtual address");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
