// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Find the device where root (/) is mounted and its name.
 * Sparkled from: https://stackoverflow.com/questions/60878209/
 */

#include <linux/kernel.h>     // printk(), pr_*()
#include <linux/module.h>     // THIS_MODULE, MODULE_VERSION, ...
#include <linux/init.h>       // module_{init,exit}
#include <linux/types.h>      // dev_t
#include <linux/kdev_t.h>     // MAJOR(), MINOR(), MKDEV()
#include <linux/path.h>       // struct path
#include <linux/namei.h>      // kern_path(), path_put()
#include <linux/stat.h>       // struct kstat, STATX_*
#include <linux/fs.h>         // vfs_getattr(), struct block_device, bdget(),...
#include <linux/version.h>    // LINUX_VERSION_CODE, KERNEL_VERSION
#include <uapi/linux/fcntl.h> // AT_*

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int __init findrootdev_init(void)
{
	struct path root_path;
	struct kstat root_stat;
	struct block_device *root_device;
	char root_device_name[BDEVNAME_SIZE];
	int err = 0;

	pr_info("init\n");

	/* NOTE:
	 *
	 * kern_path seems to take LOOKUP_* flags defined in linux/namei.h as
	 * second argument, but passing a default value of 0 is also ok.
	 *
	 * Likewise, vfs_getattr takes STATX_* flags defined in linux/stat.h as
	 * third arg. The stat() syscall passes STATX_BASIC_STATS, but passing 0
	 * should be fine too here since we do not need to know anything other
	 * than the device (->dev field), which *seems* to get populated
	 * regardless of flags.
	 */

	err = kern_path("/", 0, &root_path);
	if (err) {
		pr_err("kern_path error %d\n", err);
		goto fail;
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,10,0)
	err = vfs_getattr(&root_path, &root_stat, STATX_BASIC_STATS,
			  AT_NO_AUTOMOUNT|AT_SYMLINK_NOFOLLOW);
	if (err) {
		pr_err("vfs_getattr error %d\n", err);
		goto vfs_getattr_fail;
	}
#else /* KERNEL <= 4.10 */
	err = vfs_getattr(&root_path, &root_stat);
	if (err) {
		pr_err("vfs_getattr error %d\n", err);
		goto vfs_getattr_fail;
	}
#endif

	pr_info("root device number is 0x%08x; major = %d, minor = %d\n",
		root_stat.dev, MAJOR(root_stat.dev), MINOR(root_stat.dev));

	root_device = bdget(root_stat.dev);
	if (!root_device) {
		pr_err("bdget failed\n");
		err = -1;
		goto bdget_fail;
	}

	if (!bdevname(root_device, root_device_name)) {
		pr_err("bdevname failed\n");
		err = -1;
		goto bdevname_fail;
	}

	pr_info("root device name: %s, path: /dev/%s\n",
		root_device_name, root_device_name);

bdevname_fail:
	bdput(root_device);

bdget_fail:
vfs_getattr_fail:
	path_put(&root_path);

fail:
	return err;
}

static void __exit findrootdev_exit(void)
{
	// This function is only needed to be able to unload the module.
	pr_info("exit\n");
}

module_init(findrootdev_init);
module_exit(findrootdev_exit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Find the device where root (/) is mounted and its name.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
