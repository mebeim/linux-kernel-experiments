// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Get CPU core ID from current CPU ID.
 * Sparkled from: https://stackoverflow.com/questions/61349444
 */

#include <linux/kernel.h>       // printk(), pr_*()
#include <linux/module.h>       // THIS_MODULE, MODULE_VERSION, ...
#include <linux/init.h>         // module_{init,exit}()
#include <linux/smp.h>          // get_cpu(), put_cpu()
#include <asm/processor.h>      // cpu_data(), struct cpuinfo_x86

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int __init modinit(void)
{
	unsigned cpu;
	struct cpuinfo_x86 *info;

	cpu = get_cpu();
	info = &cpu_data(cpu);
	pr_info("CPU: %u, core: %d\n", cpu, info->cpu_core_id);
	put_cpu(); // Don't forget this!

	return 0;
}

static void __exit modexit(void)
{
	// Empty function only to be able to unload the module.
	return;
}

module_init(modinit);
module_exit(modexit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Get CPU core ID from current CPU ID.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
