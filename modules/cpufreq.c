// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Get CPU frequency for currently online CPUs.
 * Sparkled from: https://stackoverflow.com/questions/64111116
 */

#include <linux/kernel.h>  // printk(), pr_*()
#include <linux/module.h>  // THIS_MODULE, MODULE_VERSION, ...
#include <linux/init.h>    // module_{init,exit}()
#include <linux/smp.h>     // get_cpu(), put_cpu()
#include <linux/cpufreq.h> // cpufreq_get()
#include <linux/cpumask.h> // cpumasm_{first,next}() cpu_online_mask

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int __init modinit(void)
{
	unsigned cpu, freq;

	cpu = cpumask_first(cpu_online_mask);

	while (cpu < nr_cpu_ids) {
		freq = cpufreq_get(cpu);
		pr_info("CPU: %u, freq: %u kHz\n", cpu, freq);
		cpu = cpumask_next(cpu, cpu_online_mask);
	}

	return 0;
}

static void __exit modexit(void)
{
	// Empty function only to be able to unload the module.
}

module_init(modinit);
module_exit(modexit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Get CPU frequency for currently online CPUs.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
