#include <linux/init.h>             
#include <linux/module.h>          
#include <linux/kernel.h>   
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL"); 

static ssize_t kobj_attr_show(struct kobject *kobj, struct attribute *attr,
			      char *buf)
{
	printk("Call show!\n");
	return 1;
}

static ssize_t kobj_attr_store(struct kobject *kobj, struct attribute *attr,
			       const char *buf, size_t count)
{
	printk("Call store!\n");
	return count;
}
static const struct sysfs_ops static_kobj_sysfs_ops = {
	.show	= kobj_attr_show,
	.store	= kobj_attr_store,
};

static void static_kobj_release(struct kobject *kobj)
{
	printk("Call release\n");
}             

static struct kobj_type ktp = {
	.release	= static_kobj_release,
	.sysfs_ops	= &static_kobj_sysfs_ops,
};

static struct attribute foo_attr = {
	.name = "foo",
	.mode = 0664,
};
static struct attribute bar_attr = {
	.name = "bar",
	.mode = 0664,
};

static const struct attribute *attrs[] = {
	&foo_attr,
	&bar_attr,
	NULL,
};

static struct kobject kobj;
static int __init kobj_create_static_init(void)
{
	int ret = 0;
	ret = kobject_init_and_add(&kobj, &ktp, NULL, "%s", "test_kobj");
	if(ret){
		kobject_put(&kobj);
		return -EINVAL;
	}
	ret = sysfs_create_files(&kobj,attrs);
	if(ret) {
		printk("Error to create files\n");
		kobject_put(&kobj);
		return -EINVAL;
	}

	return 0;
}

static void __exit kobj_create_static_exit(void)
{
	kobject_put(&kobj);
}

module_init(kobj_create_static_init);
module_exit(kobj_create_static_exit);
