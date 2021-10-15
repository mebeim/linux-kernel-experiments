// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Calculare task RSS given a PID. Tested on kernel 5.6.
 * Usage: sudo insmod task_rss_from_pid.ko pid=123
 *        sudo modprobe tark_rss_from_pid pid=123
 * Sparkled from: https://stackoverflow.com/questions/67224020
 */

#include <linux/kernel.h>      // printk(), pr_*()
#include <linux/module.h>      // THIS_MODULE, MODULE_VERSION, ...
#include <linux/init.h>        // module_{init,exit}
#include <linux/sched/task.h>  // struct task_struct, {get,put}_task_struct()
#include <linux/mm.h>          // get_mm_rss()
#include <linux/pid.h>         // struct pid, get_pid_task(), find_get_pid()
#include <linux/moduleparam.h> // module_param_named()
#include <asm/atomic.h>        // atomic_long_read()

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int user_pid;
module_param_named(pid, user_pid, int, 0);

static int __init modinit(void)
{
	struct task_struct *tsk;
	unsigned long rss;

#ifndef CONFIG_MMU
	pr_err("No MMU, cannot calculate RSS.\n");
	return -EINVAL;
#endif

	tsk = get_pid_task(find_get_pid(user_pid), PIDTYPE_PID);
	if (tsk == NULL) {
		pr_err("No process with user PID = %d.\n", user_pid);
		return -ESRCH;
	}

	pr_info("Calculating VmRSS for \"%s\" (PID: %d)\n", tsk->comm, tsk->pid);

	if (tsk->mm) {
		rss = get_mm_rss(tsk->mm) << PAGE_SHIFT;
		pr_info("VmRSS = %lu byes\n", rss);
	} else {
		pr_info("Task is an anonymous process.\n");
	}

	put_task_struct(tsk);
	return 0;
}

static void __exit modexit(void)
{
	// This function is only needed to be able to unload the module.
}

module_init(modinit);
module_exit(modexit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Calculare task RSS given a PID.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
