// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Test waiting for a critical job to finish before rebooting or powering down.
 * Sparkled from: https://stackoverflow.com/questions/64670766
 */
#include <linux/init.h>       // module_{init,exit}()
#include <linux/module.h>     // THIS_MODULE, MODULE_VERSION, ...
#include <linux/kernel.h>     // printk(), pr_*()
#include <linux/reboot.h>     // register_reboot_notifier()
#include <linux/kthread.h>    // kthread_{create,stop,...}()
#include <linux/delay.h>      // msleep()
#include <linux/completion.h> // struct completion, complete(), ...

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static DECLARE_COMPLETION(done_wasting_time);

int my_notifier(struct notifier_block *nb, unsigned long action, void *data) {
	if (!completion_done(&done_wasting_time)) {
		pr_info("Wait! I have some critical job to finish...\n");
		wait_for_completion(&done_wasting_time);
		pr_info("Done!\n");
	}

	return NOTIFY_OK;
}

static struct notifier_block notifier = {
	.notifier_call = my_notifier,
	.next = NULL,
	.priority = 0
};

int waste_time(void *data) {
	struct completion *cmp = data;
	msleep(5000);
	complete(cmp);
	return 0;
}

static int __init modinit(void)
{
	register_reboot_notifier(&notifier);
	kthread_run(waste_time, &done_wasting_time, "waste_time");
	return 0;
}

static void __exit modexit(void)
{
	unregister_reboot_notifier(&notifier);
}

module_init(modinit);
module_exit(modexit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Test waiting for a critical job to finish before rebooting "
		   "or powering down.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
