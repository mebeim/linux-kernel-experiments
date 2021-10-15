// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Test syscall table hijacking on arm64. I must be missing some ARM knowledge
 * since this looks overcomplicated to say the least.
 * Sparkled from: https://stackoverflow.com/questions/61247838
 */

#include <linux/init.h>     // module_{init,exit}()
#include <linux/module.h>   // THIS_MODULE, MODULE_VERSION, ...
#include <linux/kernel.h>   // printk(), pr_*()
#include <linux/kallsyms.h> // kallsyms_lookup_name()
#include <asm/syscall.h>    // syscall_fn_t, __NR_*
#include <asm/ptrace.h>     // struct pt_regs
#include <asm/tlbflush.h>   // flush_tlb_all(), flush_tlb_kernel_range()
#include <asm/pgtable.h>    // {clear,set}_pte_bit(), set_pte()
#include <linux/vmalloc.h>  // vm_unmap_aliases()
#include <linux/mm.h>       // struct mm_struct, apply_to_page_range()
#include <linux/kconfig.h>  // IS_ENABLED()

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static struct mm_struct *init_mm_ptr;
static syscall_fn_t *syscall_table;
static syscall_fn_t original_read;

// From arch/arm64/mm/pageattr.c.
struct page_change_data {
	pgprot_t set_mask;
	pgprot_t clear_mask;
};

// From arch/arm64/mm/pageattr.c.
static int change_page_range(pte_t *ptep, unsigned long addr, void *data)
{
	struct page_change_data *cdata = data;
	pte_t pte = READ_ONCE(*ptep);

	pte = clear_pte_bit(pte, cdata->clear_mask);
	pte = set_pte_bit(pte, cdata->set_mask);

	set_pte(ptep, pte);
	return 0;
}

// From arch/arm64/mm/pageattr.c.
static int __change_memory_common(unsigned long start, unsigned long size,
				  pgprot_t set_mask, pgprot_t clear_mask)
{
	struct page_change_data data;
	int ret;

	data.set_mask = set_mask;
	data.clear_mask = clear_mask;

	ret = apply_to_page_range(init_mm_ptr, start, size, change_page_range,
				  &data);

	flush_tlb_kernel_range(start, start + size);
	return ret;
}

// Simplified set_memory_rw() from arch/arm64/mm/pageattr.c.
static int set_page_rw(unsigned long addr)
{
	int res;

	vm_unmap_aliases(); // Needed?

	res = __change_memory_common(addr, PAGE_SIZE, __pgprot(PTE_WRITE),
					__pgprot(PTE_RDONLY));

	// flush_tlb_all(); // Needed? Or is flush_tlb_kernel_range() enough?
	return res;
}

// Simplified set_memory_ro() from arch/arm64/mm/pageattr.c.
static int set_page_ro(unsigned long addr)
{
	int res;

	vm_unmap_aliases(); // Needed?

	res = __change_memory_common(addr, PAGE_SIZE, __pgprot(PTE_RDONLY),
					__pgprot(PTE_WRITE));

	// flush_tlb_all(); // Needed? Or is flush_tlb_kernel_range() enough?
	return res;
}

static long myread(const struct pt_regs *regs)
{
	pr_info("read() called\n");
	return original_read(regs);
}

static int __init modinit(void)
{
	int res;

	pr_info("init\n");

	// Shouldn't fail.
	init_mm_ptr = (struct mm_struct *)kallsyms_lookup_name("init_mm");
	syscall_table = (syscall_fn_t *)kallsyms_lookup_name("sys_call_table");

	original_read = syscall_table[__NR_read];

	pr_info("init_mm        @ 0x%px\n", init_mm_ptr);
	pr_info("sys_call_table @ 0x%px\n", syscall_table);
	pr_info("original_read  @ 0x%px\n", original_read);

	res = set_page_rw(((unsigned long)syscall_table + __NR_read) & PAGE_MASK);
	if (res != 0) {
		pr_err("set_page_rw() failed: %d\n", res);
		return res;
	}

	syscall_table[__NR_read] = myread;

	res = set_page_ro(((unsigned long)syscall_table + __NR_read) & PAGE_MASK);
	if (res != 0) {
		pr_err("set_page_ro() failed: %d\n", res);
		return res;
	}

	pr_info("init done\n");

	return 0;
}

static void __exit modexit(void)
{
	int res;

	pr_info("exit\n");

	res = set_page_rw(((unsigned long)syscall_table + __NR_read) & PAGE_MASK);
	if (res != 0) {
		pr_err("set_page_rw() failed: %d\n", res);
		return;
	}

	syscall_table[__NR_read] = original_read;

	res = set_page_ro(((unsigned long)syscall_table + __NR_read) & PAGE_MASK);
	if (res != 0)
		pr_err("set_page_ro() failed: %d\n", res);

	pr_info("goodbye\n");
}

module_init(modinit);
module_exit(modexit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Test syscall table hijacking on arm64.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
