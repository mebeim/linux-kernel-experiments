// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Restore old read-implies-exec kernel behavior via a kprobes hack: hook into
 * setup_new_exec() to set the READ_IMPLIES_EXEC personality flag for the
 * current task and into setup_arg_pages() to force executable_stack=true.
 * This module is written for x86_64.
 *
 * Tested on kernel 6.12.
 *
 * Sparkled from: https://stackoverflow.com/q/79249161/3889449
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/binfmts.h>

#ifndef CONFIG_X86_64
#error "This module only supports x86-64"
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int kp_setup_new_exec_pre(struct kprobe *kp, struct pt_regs *regs)
{
	current->personality |= READ_IMPLIES_EXEC;
	return 0;
}

static int kp_setup_arg_pages_pre(struct kprobe *kp, struct pt_regs *regs)
{
	regs->dx = EXSTACK_ENABLE_X;
	return 0;
}

static struct kprobe kps[] = {
	{ .pre_handler = kp_setup_arg_pages_pre, .symbol_name = "setup_arg_pages" },
	{ .pre_handler = kp_setup_new_exec_pre, .symbol_name = "setup_new_exec" },
};

static int __init modinit(void)
{
	int ret;

	for (unsigned i = 0; i < ARRAY_SIZE(kps); i++) {
		ret = register_kprobe(&kps[i]);
		if (ret < 0) {
			pr_err("Failed to register kprobe for %s: %d\n",
				kps[i].symbol_name, ret);
			return -1;
		}

		pr_info("Registered kprobe for %s\n", kps[i].symbol_name);
	}

	pr_warn("Your system now runs with old read-implies-exec semantics!\n");
	return 0;
}

static void __exit modexit(void)
{
	for (unsigned i = 0; i < ARRAY_SIZE(kps); i++) {
		unregister_kprobe(&kps[i]);
		pr_info("Unregistered kprobe for %s\n", kps[i].symbol_name);
	}
}

module_init(modinit);
module_exit(modexit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Restore old read-implies-exec behavior via a kprobes hack");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
