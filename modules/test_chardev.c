// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Test character device kernel APIs. Tested on kernel 5.8.
 */
#include <linux/kernel.h>       // printk(), pr_*()
#include <linux/module.h>       // THIS_MODULE, MODULE_VERSION, ...
#include <linux/moduleparam.h>  // module_param()
#include <linux/init.h>         // module_{init,exit}()
#include <linux/types.h>        // dev_t, loff_t, etc.
#include <linux/kdev_t.h>       // MAJOR(), MINOR(), MKDEV()
#include <linux/fs.h>           // file_operations, file, etc.
#include <linux/cdev.h>         // cdev and related functions
#include <linux/uaccess.h>      // put_user(), copy_to_user(), etc.
#include <linux/device.h>       // {device,class}_{create,destroy}() from udev
#include <linux/string.h>       // strlen()

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static dev_t devno;
static struct cdev *cdevice;
static struct class *mebeim_class;
static struct device *mebeim_device;

static int mebeim_mode = 0666;
static char* mebeim_content = "mebeim";
static size_t mebeim_content_length = 6;

module_param_named(mode, mebeim_mode, int, S_IRUGO);
module_param_named(content, mebeim_content, charp, S_IRUGO);

MODULE_PARM_DESC(mebeim_mode, "Device permissions.");
MODULE_PARM_DESC(mebeim_content,
		 "String to keep spitting out when the device is read.");

static ssize_t read_mebeim(struct file *filp, char __user *buf, size_t n,
			   loff_t *off)
{
	size_t i;
	size_t o = (size_t)*off;

	for (i = 0; i < n; i++) {
		put_user(mebeim_content[(i + o) % mebeim_content_length],
			 buf++);
	}

	*off += n;

	return n;
}

static ssize_t write_mebeim(struct file *filp, const char __user *buf, size_t n,
			    loff_t *off)
{
	return n;
}

static char *mebeim_devnode(struct device *dev, umode_t *mode)
{
	if (mode != NULL)
		*mode = (umode_t)mebeim_mode;
	return NULL;
}

static const struct file_operations mebeim_fops = {
	.read = read_mebeim,
	.write = write_mebeim
};

static int __init mychardev_init(void)
{
	int res;

	pr_debug("init\n");

	mebeim_content_length = strlen(mebeim_content);
	res = alloc_chrdev_region(&devno, 0, 1, "mebeim");

	if (res != 0) {
		pr_err("error getting dev major (%d)\n", res);
		goto fail_chrdev_alloc;
	}

	pr_debug("got dev 0x%08x major %d minor %d\n", devno, MAJOR(devno),
		 MINOR(devno));

	mebeim_class = class_create(THIS_MODULE, "mebeim");

	if (IS_ERR(mebeim_class)) {
		res =  PTR_ERR(mebeim_class);
		pr_err("error creating device class (%d)\n", res);
		goto fail_class_create;
	}

	/*
	 * This is how drivers/char/mem.c does it. See commit
	 * e454cea20bdcff10ee698d11b8882662a0153a47. Seems to only apply at
	 * mount time (i.e. if module param "mode" is made S_IWUSR, writing to
	 * /sys/module/mychardev/parameters/mode doesn't have any effect).
	 *
	 * This SO answer suggests an alternative using ->dev_uevent:
	 * https://stackoverflow.com/a/21774410/3889449
	 */
	mebeim_class->devnode = mebeim_devnode;

	cdevice = cdev_alloc();

	if (IS_ERR(cdevice)) {
		res = PTR_ERR(cdevice);
		pr_err("error allocating cdev (%d)\n", res);
		goto fail_cdev_alloc;
	}

	cdev_init(cdevice, &mebeim_fops);
	cdevice->owner = THIS_MODULE;

	res = cdev_add(cdevice, devno, 1);

	if (res != 0) {
		pr_err("error adding cdev (%d)\n", res);
		goto fail_cdev_add;
	}

	pr_debug("cdev added\n");

	mebeim_device = device_create(mebeim_class, NULL, devno, NULL,
				      "mebeim");

	if (IS_ERR(mebeim_device)) {
		res = PTR_ERR(mebeim_device);
		pr_err("error creating device (%d)\n", res);
		goto fail_device_create;
	}

	pr_debug("device created\n");
	pr_debug("init done\n");

	return 0;

fail_device_create:
fail_cdev_add:
	/*
	 * TODO: find out if this makes sense...
	 * Is cdev_del() ok to get rid of a cdev after only alloc (not add)?
	 */
	cdev_del(cdevice);
fail_cdev_alloc:
	class_destroy(mebeim_class);
fail_class_create:
	unregister_chrdev_region(devno, 1);
fail_chrdev_alloc:
	return res;
}

static void __exit mychardev_cleanup(void)
{
	pr_debug("cleanup\n");

	device_destroy(mebeim_class, devno);
	cdev_del(cdevice);
	class_destroy(mebeim_class);
	unregister_chrdev_region(devno, 1);

	pr_debug("cleanup done\n");
}

module_init(mychardev_init);
module_exit(mychardev_cleanup);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Silly character device always spitting out the same string "
		   "over and over.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
