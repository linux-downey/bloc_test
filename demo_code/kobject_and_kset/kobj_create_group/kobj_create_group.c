#include <linux/init.h>             
#include <linux/module.h>           
#include <linux/kernel.h>
#include <linux/kthread.h>      
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/gpio.h>

MODULE_LICENSE("GPL");              
MODULE_AUTHOR("Downey");      

static struct kobject *kobj;

static ssize_t bar_show(struct kobject* kobjs,struct kobj_attribute *attr,char *buf)
{
	printk("Call bar show!\n");
	return 1;
}

static ssize_t foo_show(struct kobject* kobjs,struct kobj_attribute *attr,char *buf)
{
	printk("Call foo show!\n");
	return 1;
}


static ssize_t foo_store(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count)
{
	printk("Call foo store!\n");
	return count;
}


static struct kobj_attribute foo_attr = __ATTR_RO(bar);
static struct kobj_attribute bar_attr = __ATTR(foo,0664,foo_show,foo_store); 


static struct attribute *bar_attrs[] = {
	&foo_attr.attr,
	&bar_attr.attr,
	NULL,
};


static ssize_t foobar_read(struct file *file, struct kobject *kobj,struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	printk("Call bin read!\n");
	return count; 
}

static ssize_t foobar_write(struct file *filp, struct kobject *kobj,struct bin_attribute *bin_attr,char *buf, loff_t pos, size_t count)
{
	printk("Call bin write!\n");
	return count;
}

BIN_ATTR(foobar,0664,foobar_read,foobar_write,0);

static struct bin_attribute *foobar_bin[] = {
	&bin_attr_foobar,
	NULL,
};

static struct attribute_group attr_g = {
	.name = "kobject_test",
	.attrs = bar_attrs,
	.bin_attrs = foobar_bin,
};


static int __init sysfs_ctrl_init(void){

	kobj = kobject_create_and_add("test_kobj",kernel_kobj->parent);
	if(NULL == kobj){
		printk("Failed to create kobj!\n");
		return -ENOMEM;
	}
	if(sysfs_create_group(kobj, &attr_g)){
		printk("Failed to create group!\n");
		return -EINVAL;
	}
	return 0;
}
 

static void __exit sysfs_ctrl_exit(void){
	kobject_put(kobj);
	sysfs_remove_group(kobj,&attr_g);
}
 

module_init(sysfs_ctrl_init);
module_exit(sysfs_ctrl_exit);
