// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Enumerate all the tasks that have a given PID as pid, tgid, pgid or sid.
 * Kernel/User space correspondence in nomenclature is PID/TID TGID/PID (see
 * also https://unix.stackexchange.com/a/491710/272806). Tested on kernel 5.10
 * x86_64.
 *
 * Usage: sudo insmod enum_pids.ko pid=123
 * Sparkled from: https://stackoverflow.com/questions/67235938
 *                https://stackoverflow.com/questions/71204947
 */

#include <linux/kernel.h>       // printk(), pr_*()
#include <linux/module.h>       // THIS_MODULE, MODULE_VERSION, ...
#include <linux/init.h>         // module_{init,exit}
#include <linux/moduleparam.h>  // module_param_named()
#include <linux/sched/task.h>   // struct task_struct, {get,put}_task_struct(), ...
#include <linux/sched/signal.h> // struct signal_struct, task_pid_type()
#include <linux/pid.h>          // struct pid, find_get_pid(), put_pid()
#include <linux/rculist.h>      // [h]list_for_each_entry_rcu()
#include <linux/rcupdate.h>     // rcu_read_{lock,unlock}()

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int user_pid;
module_param_named(pid, user_pid, int, 0);
MODULE_PARM_DESC(pid, "PID number to check");

static const char *types[PIDTYPE_MAX] = {"PID", "TGID", "PGID", "SID"};
static const char fmt_default[] = KERN_INFO KBUILD_MODNAME ":  -  PID=%d TGID=%d PGID=%d SID=%d \t\"%s\"";
static const char *fmts[PIDTYPE_MAX] = {
	KERN_INFO KBUILD_MODNAME":  - [PID=%d] TGID=%d PGID=%d SID=%d \"%s\"\n",
	KERN_INFO KBUILD_MODNAME":  - PID=%d [TGID=%d] PGID=%d SID=%d \"%s\"\n",
	KERN_INFO KBUILD_MODNAME":  - PID=%d TGID=%d [PGID=%d] SID=%d \"%s\"\n",
	KERN_INFO KBUILD_MODNAME":  - PID=%d TGID=%d PGID=%d [SID=%d] \"%s\"\n"
};

static void print_task(struct task_struct *task, enum pid_type type) {
	char comm[TASK_COMM_LEN];

	get_task_struct(task);
	printk(fmts[type] ?: fmt_default,
		pid_nr(task_pid_type(task, PIDTYPE_PID)),  // task_pid_nr(task)
		pid_nr(task_pid_type(task, PIDTYPE_TGID)), // task_tgid_nr(task)
		pid_nr(task_pid_type(task, PIDTYPE_PGID)),
		pid_nr(task_pid_type(task, PIDTYPE_SID)),
		get_task_comm(comm, task));
	put_task_struct(task);
}

static void print_all_threads(struct task_struct *task, enum pid_type type) {
	struct task_struct *thread;

	/* Threads are in the linked list task->signal->thread_head and
	 * task_struct has a ->thread_node field for this list. Not sure why
	 * thread_head is in ->signal, maybe because iterating over threads is
	 * only done for signal delivery purposes, and it's best to avoid adding
	 * weight to task_struct.
	 */

	get_task_struct(task);
	list_for_each_entry_rcu(thread, &task->signal->thread_head, thread_node)
		print_task(thread, type);
	put_task_struct(task);
}

static int __init modinit(void)
{
	struct task_struct *task;
	struct pid *pid;
	int type;
	bool any;

	pid = find_get_pid(user_pid);
	if (!pid) {
		pr_err("No such pid (%d).\n", user_pid);
		return -ESRCH;
	}

	pr_info("Enumerating tasks having pid nr %d for each pid type\n", user_pid);

	/* task_struct has one hlist_node for pid type:
	 *
	 *     struct hlist_node pid_links[PIDTYPE_MAX];
	 */

	rcu_read_lock();

	for (type = 0; type < PIDTYPE_MAX; type++) {
		pr_info("Tasks with %s=%d:\n", types[type] ?: "<unknown>", user_pid);
		any = false;

		/* Confusingly enough, `pid->tasks[PIDTYPE_TGID]` does not hold
		 * all tasks having `pid` as TGID, so we'll have to
		 * differentiate here and iterate over threads manually for
		 * PIDTYPE_TGID. There are also some conveniance macros
		 * in linux/pid.h ({do,while}_each_pid_{task,thread}), but
		 * whatever.
		 */

		hlist_for_each_entry_rcu(task, &pid->tasks[type], pid_links[type]) {
			any = true;

			if (type == PIDTYPE_TGID)
				print_all_threads(task, type);
			else
				print_task(task, type);
		}

		if (!any)
			pr_info("  - none\n");
	}

	rcu_read_unlock();
	put_pid(pid);

	// Just fail loading with a random error to make it simpler to use this
	// module multiple times in a row.
	return -ECANCELED;
}

module_init(modinit);
MODULE_VERSION("0.2");
MODULE_DESCRIPTION("Enumerate all tasks using a given pid (kernel struct pid).");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
