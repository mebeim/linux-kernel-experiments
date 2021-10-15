// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Test the undefined instruction handler in arm64.
 * Sparkled from: https://stackoverflow.com/questions/61238959
 */

#include <linux/init.h>     // module_{init,exit}()
#include <linux/module.h>   // THIS_MODULE, MODULE_VERSION, ...
#include <asm/traps.h>      // struct undef_hook
#include <asm/ptrace.h>     // struct pt_regs
#include <linux/kallsyms.h> // kallsyms_lookup_name()

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static void (*register_undef_hook_ptr)(struct undef_hook *);
static void (*unregister_undef_hook_ptr)(struct undef_hook *);
static int undef_instr_handler(struct pt_regs *, u32);

struct undef_hook uh = {
	.instr_mask  = 0x0,
	.instr_val   = 0x0,
	.pstate_mask = 0x0,
	.pstate_val  = 0x0,
	.fn          = undef_instr_handler
};

void whoops(void)
{
	// Execute a known invalid instruction.
	asm volatile (".word 0xf7f0a000");
}

static int undef_instr_handler(struct pt_regs *regs, u32 instr)
{
	pr_info("*gotcha*\n");

	// Jump over to next instruction.
	regs->pc += 4;

	return 0; // All fine!
}

static int __init modinit(void)
{
	register_undef_hook_ptr   = (void *)kallsyms_lookup_name("register_undef_hook");
	unregister_undef_hook_ptr = (void *)kallsyms_lookup_name("unregister_undef_hook");

	if (!register_undef_hook_ptr)
		return -EFAULT;

	register_undef_hook_ptr(&uh);

	pr_info("Jumping off a cliff...\n");
	whoops();
	pr_info("Woah, I survived!\n");

	return 0;
}

static void __exit modexit(void)
{
	if (unregister_undef_hook_ptr)
		unregister_undef_hook_ptr(&uh);
}

module_init(modinit);
module_exit(modexit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Test the undefined instruction handler in arm64.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
