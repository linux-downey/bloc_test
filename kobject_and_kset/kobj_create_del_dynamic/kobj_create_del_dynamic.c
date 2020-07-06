#include <linux/init.h>             
#include <linux/module.h>          
#include <linux/kernel.h>   
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL"); 


static ssize_t foo_show(struct kobject *kobj,struct kobj_attribute *attr,char *buf)
{
	printk("Call foo show!\n");
	return 1;
}

static ssize_t bar_show(struct kobject *kobj,struct kobj_attribute *attr,char *buf)
{
	printk("Call bar show!\n");
	return 1;
}

static ssize_t foo_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t count)
{
	printk("Call foo store!\n");
	return count;
}

static ssize_t bar_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t count)
{
	printk("Call bar store!\n");
	return count;
}



struct kobj_attribute foo_attr = __ATTR(foo, 0644, foo_show, foo_store);
struct kobj_attribute bar_attr = __ATTR(bar, 0644, bar_show, bar_store);


static const struct attribute *attrs[] = {
	&foo_attr.attr,
	&bar_attr.attr,
	NULL,
};

static struct kobject *kobj;
static int __init kobj_create_static_init(void)
{
	int ret = 0;
	kobj = kobject_create_and_add("test_kobj", NULL);
	if(NULL == kobj){
		printk("Failed to create kobj\n");
		return -EINVAL;
	}
	ret = sysfs_create_files(kobj,attrs);
	if(ret) {
		printk("Error to create files\n");
		kobject_put(kobj);
		return -EINVAL;
	}

	return 0;
}

static void __exit kobj_create_static_exit(void)
{
	kobject_put(kobj);
}

module_init(kobj_create_static_init);
module_exit(kobj_create_static_exit);
