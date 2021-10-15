// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Get current date and time from kernel space taking into account time zone.
 */

#include <linux/init.h>        // module_{init,exit}()
#include <linux/module.h>      // THIS_MODULE, MODULE_VERSION, ...
#include <linux/kernel.h>      // printk(), pr_*()
#include <linux/timekeeping.h> // ktime_get_real_seconds(), ...

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int __init datetime_init(void)
{
	struct tm t;

	pr_info("sys_tz.tz_minuteswest = %d\n", sys_tz.tz_minuteswest);
	pr_info("sys_tz.tz_dsttime = %d\n", sys_tz.tz_dsttime);

	time64_to_tm(ktime_get_real_seconds(), -sys_tz.tz_minuteswest * 60, &t);

	pr_info("Date and time: %0ld-%02d-%02d %02d:%02d:%02d\n",
		t.tm_year + 1900,
		t.tm_mon + 1,
		t.tm_mday,
		t.tm_hour,
		t.tm_min,
		t.tm_sec
	);

	return 0;
}

static void __exit datetime_exit(void)
{
	return;
}

module_init(datetime_init);
module_exit(datetime_exit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Get current date and time from kernel space.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
