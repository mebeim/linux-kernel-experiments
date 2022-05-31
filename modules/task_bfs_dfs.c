// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Iterate and dump a task's children tree using BFS or DFS.
 * Tested on kernel 5.10, 5.17.
 * Usage: sudo insmod task_bfs.ko pid=123
 *        sudo modprobe task_bfs pid=123
 * Sparkled from: https://stackoverflow.com/questions/61201560 (sadly deleted by
 * the user), also posted here: https://stackoverflow.com/questions/19208487.
 */

#include <linux/kernel.h>      // printk(), pr_*()
#include <linux/module.h>      // THIS_MODULE, MODULE_VERSION, ...
#include <linux/init.h>        // module_{init,exit}
#include <linux/list.h>        // struct list_head, list_for_each()
#include <linux/sched/task.h>  // struct task_struct, {get,put}_task_struct()
#include <linux/slab.h>        // kmalloc(), kfree()
#include <linux/pid.h>         // struct pid, get_pid_task(), find_get_pid()
#include <linux/version.h>     // LINUX_VERSION_CODE, KERNEL_VERSION
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)
#define TASK_STATE_FMT "%u"
static inline unsigned task_state(const struct task_struct *task) {
	return READ_ONCE(task->__state);
}
#else
#define TASK_STATE_FMT "%ld"
static inline long task_state(const struct task_struct *task) {
	return task->state;
}
#endif


// Undefine to use DFS instead
#define BFS
#ifdef BFS
#define TECHNIQUE "BFS"
#else
#define TECHNIQUE "DFS"
#endif

static void dump_children_tree(struct task_struct *task)
{
	char comm[TASK_COMM_LEN];
	struct task_struct *child;
	struct list_head *list;
	struct queue *q, *tmp;
#ifdef BFS
	struct queue **tail;
#endif
	pid_t ppid;

	q = kmalloc(sizeof *q, GFP_KERNEL);
	if (!q)
		goto out_nomem;

	q->task = task;
	q->next = NULL;
#ifdef BFS
	tail = &q->next;
#endif

	while (q) {
		task = q->task;
#ifndef BFS
		tmp = q;
		q = q->next;
		kfree(tmp);
#endif

		rcu_read_lock();
		ppid = rcu_dereference(task->real_parent)->pid;
		rcu_read_unlock();

		pr_info("Name: %-20s State: "TASK_STATE_FMT"\tPID: %d\tPPID: %d\n",
			get_task_comm(comm, task), task_state(task), task->pid,
			ppid);

		list_for_each(list, &task->children) {
			child = list_entry(list, struct task_struct, sibling);
			get_task_struct(child);

			tmp = kmalloc(sizeof *tmp, GFP_KERNEL);
			if (!tmp)
				goto out_nomem;

			tmp->task = child;
#ifdef BFS
			tmp->next = NULL;
			*tail = tmp;
			tail = &tmp->next;
#else // DFS
			tmp->next = q;
			q = tmp;
#endif
		}

		put_task_struct(task);
#ifdef BFS
		tmp = q;
		q = q->next;
		kfree(tmp);
#endif
	}

	return;

out_nomem:
	while (q) {
		tmp = q;
		q = q->next;
		kfree(tmp);
	}
}

static int __init modinit(void)
{
	char comm[TASK_COMM_LEN];
	struct task_struct *tsk;

	tsk = get_user_pid_task(user_pid);
	if (tsk == NULL) {
		pr_err("No process with user PID = %d.\n", user_pid);
		return -ESRCH;
	}

	pr_info("Running %s on task \"%s\" (PID: %d)\n", TECHNIQUE,
		get_task_comm(comm, tsk), tsk->pid);

	dump_children_tree(tsk);

	// Just fail loading with a random error to make it simpler to use this
	// module multiple times in a row.
	return -ECANCELED;
}

module_init(modinit);
MODULE_VERSION("0.3");
MODULE_DESCRIPTION("Iterate over a task's children using BFS.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
