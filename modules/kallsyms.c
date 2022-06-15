// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Lookup and grep kallsyms from kernel space. Note that this module depends on
 * kallsyms_on_each_symbol(), which needs CONFIG_LIVEPATCH=y on kernel >= 5.12.
 *
 * Tested on kernel 4.19, 5.10, 5.18 (x86-64); 5.4 (arm64).
 *
 * Changelog:
 *
 * v0.2: Support kernel >= v5.7 using kprobes to find kallsyms_on_each_symbol()
 *       even if not exported.
 * v0.1: Initial version.
 */

#include <linux/init.h>         // module_{init,exit}()
#include <linux/module.h>       // THIS_MODULE, MODULE_VERSION, ...
#include <linux/kernel.h>       // printk(), pr_*()
#include <linux/moduleparam.h>  // module_param()
#include <linux/string.h>       // strcmp()
#include <linux/version.h>      // LINUX_VERSION_CODE, KERNEL_VERSION()

// Since v5.7, kallsyms_on_each_symbol() and others aren't exported anymore.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
#include <linux/kprobes.h>      // [un]register_kprobe
#define KALLSYMS_NOT_EXPORTED
#else
#include <linux/kallsyms.h>     // kallsyms_on_each_symbol()
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static char *find = NULL;
module_param(find, charp, 0);

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
	static int (*pkallsyms_on_each_symbol)(int (*fn)(void *, const char *, struct module *, unsigned long), void *);

#ifdef KALLSYMS_NOT_EXPORTED
	// Find the symbol through a kprobes hack
	struct kprobe kp = { .symbol_name = "kallsyms_on_each_symbol" };
	int err;

	err = register_kprobe(&kp);
	if (err) {
		pr_err("register_kprobe() failed: %d\n", err);
		return err;
	}

	pkallsyms_on_each_symbol = (typeof(pkallsyms_on_each_symbol))kp.addr;
	unregister_kprobe(&kp);
#else
	pkallsyms_on_each_symbol = &kallsyms_on_each_symbol;
#endif

	pkallsyms_on_each_symbol(find ? grep_ksym : print_ksym, NULL);

	// Just fail loading with a random error to make it simpler to use this
	// module multiple times in a row.
	return -ECANCELED;
}

module_init(modinit);
MODULE_VERSION("0.2");
MODULE_DESCRIPTION("Kernel symbol list/search utility.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
