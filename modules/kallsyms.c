// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Lookup kallsyms from kernel space.
 */

#include <linux/init.h>         // module_{init,exit}()
#include <linux/module.h>       // THIS_MODULE, MODULE_VERSION, ...
#include <linux/kernel.h>       // printk(), pr_*()
#include <linux/moduleparam.h>  // module_param()
#include <linux/kallsyms.h>     // kallsyms_on_each_symbol()
#include <linux/string.h>       // strcmp()
#include <linux/version.h>      // LINUX_VERSION_CODE, KERNEL_VERSION()

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
#error "since Linux v5.7, kallsyms_on_each_symbol() is not exported anymore"
#endif

static char *find = NULL;
module_param(find, charp, S_IRUGO);

static int print_ksym(void *_, const char *name, struct module *module, unsigned long addr)
{
	if (module && module->name[0])
		pr_info("0x%px %s [%s]\n", (void*)addr, name, module->name);
	else
		pr_info("0x%px %s\n", (void*)addr, name);
	return 0;
}

static int grep_ksym(void *_, const char *name, struct module *module, unsigned long addr)
{
	if (strstr(name, find))
		return print_ksym(NULL, name, module, addr);
	return 0;
}

static int __init modinit(void)
{
	kallsyms_on_each_symbol(find ? grep_ksym : print_ksym, NULL);
	// Just fail loading with a random error to make it simpler to use this
	// module multiple times in a row.
	return -ECANCELED;
}

module_init(modinit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Kernel symbol list/search utility.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
