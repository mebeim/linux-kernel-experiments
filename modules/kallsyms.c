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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static char *find = NULL;
module_param(find, charp, S_IRUGO);

static int print_ksym(void *data, const char *name, struct module *module, unsigned long addr)
{
	pr_info("0x%px  %s\n", (void*)addr, name);
	return 0;
}

static int grep_ksym(void *data, const char *name, struct module *module, unsigned long addr)
{
	if (strstr(name, find))
		return print_ksym(data, name, module, addr);
	return 0;
}

static int __init modinit(void)
{
	kallsyms_on_each_symbol(find ? grep_ksym : print_ksym, NULL);
	return -EXFULL; // Return with random error just to make things faster.
}

static void __exit modexit(void)
{
	return;
}

module_init(modinit);
module_exit(modexit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Kernel symbol list/search utility.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
