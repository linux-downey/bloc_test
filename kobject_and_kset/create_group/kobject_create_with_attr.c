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
MODULE_DESCRIPTION("Kobject test!");  
MODULE_VERSION("0.1");              

static int led_status = 0;
#define LED_PIN   26
/*************************kobject***************************/
static struct kobject *kob;

static ssize_t led_show(struct kobject* kobjs,struct kobj_attribute *attr,char *buf)
{
    printk(KERN_INFO "Read led\n");
    return sprintf(buf,"The led_status status = %d\n",led_status);
}

static ssize_t led_status_show(struct kobject* kobjs,struct kobj_attribute *attr,char *buf)
{
    printk(KERN_INFO "led status show\n");
    return sprintf(buf,"led status : \n%d\n",led_status);
}


static ssize_t led_status_store(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count)
{
    printk(KERN_INFO "led status store\n");
    if(0 == memcmp(buf,"on",2))
    {
        gpio_set_value(LED_PIN,1);
        led_status = 1;
    }
    else if(0 == memcmp(buf,"off",3))
    {
        gpio_set_value(LED_PIN,0);
        led_status = 0;
    }
    else
    {
        printk(KERN_INFO "Not support cmd\n");
    }
    
    return count;
}


static struct kobj_attribute status_attr = __ATTR_RO(led);
static struct kobj_attribute led_attr = __ATTR(led_status,0660,led_status_show,led_status_store);  //Doesn't support 0666 in new version.


static struct attribute *led_attrs[] = {
    &status_attr.attr,
    &led_attr.attr,
    NULL,
};

static struct attribute_group attr_g = {
    .name = "kobject_test",
    .attrs = led_attrs,
};


int create_kobject(void)
{
    kob = kobject_create_and_add("obj_test",kernel_kobj->parent);
    return 0;
}

static void gpio_config(void)
{
    if(!gpio_is_valid(LED_PIN)){
        printk(KERN_ALERT "Error wrong gpio number\n");
        return ;
    }
    gpio_request(LED_PIN,"led_ctr");
    gpio_direction_output(LED_PIN,1);
    gpio_set_value(LED_PIN,1);
    led_status = 1;
}

static void gpio_deconfig(void)
{
    gpio_free(LED_PIN);
}

static int __init sysfs_ctrl_init(void){
    printk(KERN_INFO "Kobject test!\n");
    gpio_config();
    create_kobject();
    sysfs_create_group(kob, &attr_g);
    return 0;
}
 

static void __exit sysfs_ctrl_exit(void){

    gpio_deconfig();
    kobject_put(kob);
    printk(KERN_INFO "Goodbye!\n");
}
 

module_init(sysfs_ctrl_init);
module_exit(sysfs_ctrl_exit);
