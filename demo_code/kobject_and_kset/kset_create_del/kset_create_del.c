#include <linux/init.h>             
#include <linux/module.h>          
#include <linux/kernel.h>   
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");              

static struct kset *kst;

static int __init create_file_init(void)
{
	kst = kset_create_and_add("test_kset",NULL,NULL);
	if(!kst)
	{
		printk(KERN_ALERT "Create kset failed\n");
		kset_put(kst);
	}

	return 0;
}

static void __exit create_file_exit(void)
{
	kset_unregister(kst);
}

module_init(create_file_init);
module_exit(create_file_exit);
