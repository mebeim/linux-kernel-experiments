// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Iterate over a task's children using BFS. Tested on kernel 4.19.
 * Usage: sudo insmod task_bfs.ko pid=123
 *        sudo modprobe task_bfs pid=123
 * Sparkled from: https://stackoverflow.com/questions/61201560 (sadly deleted by
 * the user)
 */

#include <linux/kernel.h>      // printk(), pr_*()
#include <linux/module.h>      // THIS_MODULE, MODULE_VERSION, ...
#include <linux/init.h>        // module_{init,exit}
#include <linux/list.h>        // struct list_head, list_for_each()
#include <linux/sched.h>       // struct task_struct, {get,put}_task_struct()
#include <linux/slab.h>        // kmalloc(), kfree()
#include <linux/pid.h>         // struct pid, get_pid_task(), find_get_pid()
#include <linux/moduleparam.h> // module_param_named()

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int user_pid;
module_param_named(pid, user_pid, int, 0);

struct queue {
	struct task_struct *task;
	struct queue *next;
};

/**
 * Find task_struct given **userspace** PID.
 *
 * NOTE: caller must put_task_struct() when done.
 */
static struct task_struct *get_user_pid_task(pid_t pid) {
	return get_pid_task(find_get_pid(pid), PIDTYPE_PID);
}

static void bfs(struct task_struct *task) {
	struct task_struct *child;
	struct list_head *list;
	struct queue *q, *tmp, **tail;

	q = kmalloc(sizeof *q, GFP_KERNEL);
	q->task = task;
	q->next = NULL;
	tail = &q->next;

	while (q != NULL) {
		task = q->task;

		pr_info("Name: %-20s State: 0x%lx\tPID: %d\n",
			task->comm,
			task->state,
			task->pid
		);

		list_for_each(list, &task->children) {
			child = list_entry(list, struct task_struct, sibling);
			get_task_struct(child);

			tmp = kmalloc(sizeof *tmp, GFP_KERNEL);
			tmp->task = child;
			tmp->next = NULL;

			*tail = tmp;
			tail = &tmp->next;
		}

		put_task_struct(task);

		tmp = q;
		q = q->next;
		kfree(tmp);
	}
}

static int __init modinit(void)
{
	struct task_struct *tsk;

	pr_info("init\n");

	tsk = get_user_pid_task(user_pid); // Automatically calls get_task_struct(tsk) for us.
	if (tsk == NULL) {
		pr_err("No process with user PID = %d.\n", user_pid);
		return -ESRCH; // No such process.
	}

	pr_info("Running BFS on task \"%s\" (PID: %d)\n", tsk->comm, tsk->pid);

	bfs(tsk); // Automatically calls put_task_struct(tsk) for us.

	return 0;
}

static void __exit modexit(void)
{
	// This function is only needed to be able to unload the module.
	pr_info("exit\n");
}

module_init(modinit);
module_exit(modexit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Iterate over a task's children using BFS.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
