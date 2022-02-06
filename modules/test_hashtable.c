// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/**
 * Test kernel hashtable API.
 * Sparkled from: https://stackoverflow.com/questions/60870788
 */

#include <linux/kernel.h>    // printk(), pr_*()
#include <linux/module.h>    // THIS_MODULE, MODULE_VERSION, ...
#include <linux/init.h>      // module_{init,exit}
#include <linux/hashtable.h> // hashtable API
#include <linux/string.h>    // strcpy, strcmp
#include <linux/types.h>     // u32 etc.

#ifdef CONFIG_XXHASH
#include <linux/xxhash.h>    // xxhash
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

struct mystruct {
	int data;
	char *name;
	struct hlist_node node;
};

DECLARE_HASHTABLE(tbl, 3);

#ifndef CONFIG_XXHASH
static u32 mystupidhash(const char *s) {
	u32 key = 0;
	char c;

	while ((c = *s++))
		key += c;

	return key;
}
#endif

static int __init myhashtable_init(void)
{
	struct mystruct a, b, *cur;
	u32 key_a, key_b;
	unsigned bkt;

	pr_info("module loaded\n");

	a.data = 3;
	a.name = "foo";
	b.data = 7;
	b.name = "oof";

	/* Calculate keys. Beware that they are not unique, and even if so, the
	 * hash_add() function could pick the same bucket index.
	 */

#ifdef CONFIG_XXHASH
	// If kernel was configured with CONFIG_XXHASH, we can use xxhash()
	key_a = xxhash(a.name, strlen(a.name), 0x1337c0febabe1337);
	key_b = xxhash(b.name, strlen(b.name), 0x1337c0febabe1337);
#else
	key_a = mystupidhash(a.name);
	key_b = mystupidhash(b.name);
#endif

	pr_info("key_a = %u, key_b = %u\n", key_a, key_b);

	// Insert elements.
	hash_init(tbl);
	hash_add(tbl, &a.node, key_a);
	hash_add(tbl, &b.node, key_b);

	// List all elements in the table.
	hash_for_each(tbl, bkt, cur, node) {
		pr_info("element: data = %d, name = %s\n",
			cur->data, cur->name);
	}

	// Get the element with name = "foo".
	hash_for_each_possible(tbl, cur, node, key_a) {
		pr_info("match for key %u: data = %d, name = %s\n",
			key_a, cur->data, cur->name);

		// Need to check the name in case of collision.
		if (!strcmp(cur->name, "foo")) {
			pr_info("element named \"foo\" found!\n");
			break;
		}
	}

	// Remove elements.
	hash_del(&a.node);
	hash_del(&b.node);

	return 0;
}

static void __exit myhashtable_exit(void)
{
	// This function is only needed to be able to unload the module.
	pr_info("module unloaded\n");
}


module_init(myhashtable_init);
module_exit(myhashtable_exit);
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Test kernel hashtable API.");
MODULE_AUTHOR("Marco Bonelli");
MODULE_LICENSE("Dual MIT/GPL");
