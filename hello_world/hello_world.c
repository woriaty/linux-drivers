#include <linux/init.h>
#include <linux/module.h>

static int __init hello_init(void)
{
	printk(KERN_INFO"hello_world!!!\n");
	return 0;
}

module_init(hello_init);

static void __exit hello_exit(void)
{
	printk(KERN_INFO"goodbye!!!\n");
}

module_exit(hello_exit);

MODULE_AUTHOR("woriaty @ ubuntu");
MODULE_LICENSE("GPL_v2");
MODULE_DESCRIPTION("a simple hello world module");
MODULE_ALIAS("simple module");
