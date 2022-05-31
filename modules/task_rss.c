// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Calculare task RSS of all running tasks.
 * Tested on kernel 5.6 arm64, 5.10 x86_64, 5.17 x86_64.
 * Usage: sudo insmod task_rss
 *        sudo modprobe task_rss
 * Sparkled from: https://stackoverflow.com/questions/67224020
 */

#include <linux/kernel.h>       // printk(), pr_*()
#include <linux/module.h>       // THIS_MODULE, MODULE_VERSION, ...
#include <linux/init.h>         // module_{init,exit}
#include <linux/sched/task.h>   // struct task_struct, {get,put}_task_struct()
#include <linux/sched/signal.h> // for_each_process()
#include <linux/sched/mm.h>     // get_task_mm(), mmput()
#include <linux/mm.h>           // get_mm_rss()

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int __init modinit(void)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	unsigned long rss;
	char comm[TASK_COMM_LEN];

#ifndef CONFIG_MMU
	pr_err("No MMU, cannot calculate RSS.\n");
	return -EINVAL;
#endif

	for_each_process(tsk) {
		get_task_struct(tsk);
		get_task_comm(comm, tsk);
		mm = get_task_mm(tsk);

		// https://www.kernel.org/doc/Documentation/vm/active_mm.rst
		if (mm) {
			rss = get_mm_rss(mm) << PAGE_SHIFT;
			mmput(mm);
			pr_info("PID %d (\"%s\") VmRSS = %lu bytes\n", tsk->pid, comm, rss);
		} else {
			pr_info("PID %d (\"%s\") is an anonymous process\n", tsk->pid, comm);
		}

		put_task_struct(tsk);
	}

	// Just fail loading with a random error to make it simpler to use this
	// module multiple times in a row.
	return -ECANCELED;
}

module_init(modinit);
MODULE_VERSION("0.2");
MODULE_DESCRIPTION("Calculare task RSS of all running tasks.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
