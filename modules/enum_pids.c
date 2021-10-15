// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Enumerate all the tasks that have a given PID as pid, tgid, pgid or sid.
 * Tested on kernel 5.6
 * Usage: sudo insmod enum_pids.ko pid=123
 * Sparkled from: https://stackoverflow.com/questions/67235938
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
	struct task_struct *task;
	struct pid *pid;
	int type;

	pr_info("Enumerating tasks with pid=%d\n", user_pid);
	pr_info("Types: PID=%d, TGID=%d, PGID=%d, SID=%d\n",
		PIDTYPE_PID, PIDTYPE_TGID, PIDTYPE_PGID, PIDTYPE_SID);

	pid = find_get_pid(user_pid);
	if (!pid) {
		pr_err("No such pid (%d).\n", user_pid);
		return -ESRCH;
	}

	// task_struct has one hlist_node for pid type:
	//     struct hlist_node pid_links[PIDTYPE_MAX];

	for (type = 0; type < PIDTYPE_MAX; type++) {
		pr_info("Type %d...\n", type);

		hlist_for_each_entry(task, &pid->tasks[type], pid_links[type]) {
			get_task_struct(task);
			pr_info("  - pid=%d tgid=%d (\"%s\")",
				task->pid, task->tgid, task->comm);
			put_task_struct(task);
		}
	}

	pr_info("Done.\n");
	return 0;
}

static void __exit modexit(void)
{
	// This function is only needed to be able to unload the module.
}

module_init(modinit);
module_exit(modexit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Enumerate all tasks using a given pid (kernel struct pid).");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
