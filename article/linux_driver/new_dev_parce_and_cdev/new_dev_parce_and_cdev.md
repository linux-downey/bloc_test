# 创建设备程序的第二种写法 —— cdev
## 回顾
在上一章中，我们介绍了怎么创建设备节点，然后通过操作指定的设备节点来调用内核模块中file_operations结构体中对应的读写函数，创建设备节点的操作是这样的：
* 调用register_chrdev()在内核中创建一个字符设备
* 调用class_create()创建一个/sys中对应的class
* 调用device_create()在/Dev目录下创建一个设备节点文件
接下来我们来看看字符设备驱动的另一种写法。  

## 创建字符设备另一种写法
程序实现如下：  
```C
#include <linux/init.h>  
#include <linux/module.h>
#include <linux/device.h>  
#include <linux/kernel.h>  
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

MODULE_AUTHOR("Downey");
MODULE_LICENSE("GPL");

static int majorNumber = 0;
/*Class 名称，对应/sys/class/下的目录名称*/
static const char *CLASS_NAME = "basic_class";
/*Device 名称，对应/dev下的目录名称*/
static const char *DEVICE_NAME = "basic_demo";

static int basic_open(struct inode *node, struct file *file);
static ssize_t basic_read(struct file *file,char *buf, size_t len,loff_t *offset);
static ssize_t basic_write(struct file *file,const char *buf,size_t len,loff_t* offset);
static int basic_release(struct inode *node,struct file *file);


static char msg[] = "Downey!";
static char recv_msg[20];

static struct class *basic_class = NULL;
static struct device *basic_device = NULL;

/*File opertion 结构体，我们通过这个结构体建立应用程序到内核之间操作的映射*/
static struct file_operations file_oprts = 
{
    .open = basic_open,
    .read = basic_read,
    .write = basic_write,
    .release = basic_release,
};

static dev_t dev_id = 0;
static int ret = 0;
static struct cdev basic_cdev;

static int __init basic_init(void)
{
    printk(KERN_ALERT "Driver init\r\n");
    /*创建设备号，如果主设备号已被指定，就需要指定次设备号*/
    if(majorNumber){
        dev_id = MKDEV(majorNumber,0);
        ret = register_chrdev_region(dev_id,2,DEVICE_NAME);
        if(ret){
            printk(KERN_ALERT "Register chrdev region failed!!\r\n");
            return -ENOMEM;
        }
    }
    /*如果没有指定主设备号，就申请一个主设备号*/
    else{
        ret = alloc_chrdev_region(&dev_id,0,2,DEVICE_NAME);
        if(ret){

            printk(KERN_ALERT "Alloc chrdev region failed!!\r\n");
            return -ENOMEM;
        }
        majorNumber = MAJOR(dev_id);
    }
    /*初始化一个cdev*/
    cdev_init(&basic_cdev,&file_oprts);
    /*添加一个cdev*/
    ret = cdev_add(&basic_cdev,dev_id,2);
    basic_class = class_create(THIS_MODULE,CLASS_NAME);
    /*新创建一个设备节点*/
    basic_device = device_create(basic_class, NULL, MKDEV(majorNumber,0), NULL, DEVICE_NAME);
    printk(KERN_ALERT "Basic device init success!!\r\n");

    return 0;
}

/*当用户打开这个设备文件时，调用这个函数*/
static int basic_open(struct inode *node, struct file *file)
{
    printk(KERN_ALERT "Open file\r\n");
    return 0;
}

/*当用户试图从设备空间读取数据时，调用这个函数*/
static ssize_t basic_read(struct file *file,char *buf, size_t len,loff_t *offset)
{
    int cnt = 0;
    /*将内核空间的数据copy到用户空间*/
    cnt = copy_to_user(buf,msg,sizeof(msg));
    if(0 == cnt){
        printk(KERN_INFO "Send file!!\n");
        return 0;
    }
    else{
        printk(KERN_ALERT "ERROR occur when reading!!\n");
        return -EFAULT;
    }
    return sizeof(msg);
}

/*当用户往设备文件写数据时，调用这个函数*/
static ssize_t basic_write(struct file *file,const char *buf,size_t len,loff_t *offset)
{
    /*将用户空间的数据copy到内核空间*/
    int cnt = copy_from_user(recv_msg,buf,len);
    if(0 == cnt){
        printk(KERN_INFO "Recieve file!!\n");
    }
    else{
        printk(KERN_ALERT "ERROR occur when writing!!\n");
        return -EFAULT;
    }
    printk(KERN_INFO "Recive data ,len = %s\n",recv_msg);
    return len;
}

/*当用户打开设备文件时，调用这个函数*/
static int basic_release(struct inode *node,struct file *file)
{
    printk(KERN_INFO "Release!!\n");
    return 0;
}

/*销毁注册的所有资源，卸载模块，这是保持linux内核稳定的重要一步*/
static void __exit basic_exit(void)
{
    device_destroy(basic_class,MKDEV(majorNumber,0));
    class_unregister(basic_class);
    class_destroy(basic_class);
    cdev_del(&basic_cdev);
    unregister_chrdev_region(dev_id,2);
}

module_init(basic_init);
module_exit(basic_exit);
```

对比上一章节的驱动程序，唯一的区别就是从内核注册设备号，在上一章节中，我们直接使用register_chrdev()来注册设备号，通过传入一个主设备号，如果主设备号为0则由系统分配一个主设备号，然后再将设备号与file_operation结构体作关联。  
而在这一章节中，我们使用了register_chrdev_region()或者alloc_chrdev_region()来创建设备号，然后使用cdev和cdev_add来将设备号和file_operation结构体作关联。  
那么，他们之间有什么区别呢？  
拿alloc_chrdev_region()举例，它的原型为：

    int alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count,
const char *name)
* 第一个参数为dev_t*类型，dev_t为设备号类型，它实际上是一个32位类型，12位表示主设备号，20位表示次设备号。   
    主设备号一般用来区分不同种类的设备，次设备号一般用来区分同类设备中不同的多个设备。
* 第二个参数为baseminor表示次设备号的起始值
* 第三个参数为申请以起始值开始的count个次设备号。
在上面的程序中，alloc_chrdev_region(&dev_id,0,2,DEVICE_NAME)中参数(0,2)表示申请从次设备0开始之后的两个次设备号，即0和1，当我们在/dev下新创建驱动设备节点时会指定主次设备：

    device_create(basic_class, NULL, MKDEV(majorNumber,0), NULL, DEVICE_NAME);
同时，我们也可以指定次设备号为1，创建另一个设备节点文件，比如名称为DEVICE_NAME1

    device_create(basic_class, NULL, MKDEV(majorNumber,1), NULL, DEVICE_NAME);
但是，如果我们要以当前主设备号和次设备号2来创建一个节点，例如这样：

    device_create(basic_class, NULL, MKDEV(majorNumber,2), NULL, DEVICE_NAME);
这是不行的，在向内核申请设备节点时，只申请了次设备0,1，所以当前的file_operation结构体并没有绑定到设备号为MKDEV(majorNumber,2)的设备上。  

我们不妨再回过头来看看上一章节中的register_chrdev()做了什么，它的源代码是这样的：

    static inline int register_chrdev(unsigned int major, const char *name,
				  const struct file_operations *fops)
    {
        return __register_chrdev(major, 0, 256, name, fops);
    }
原来它就是自动将0-255的次设备号分配给了当前设备驱动，然后我们再追踪__register_chrdev函数，它的部分实现是这样的：

    cd = __register_chrdev_region(major, baseminor, count, name);
    cdev = cdev_alloc();
    err = cdev_add(cdev, MKDEV(cd->major, baseminor), count);
而__register_chrdev的底层依旧是cdev机制实现的。  
## 结论
不管是使用register_chrdev()来直接申请设备号还是使用cdev的方式来申请设备号，它的底层实现机制都是cdev机制来实现的，而使用cdev机制能更灵活精确地控制主次设备号的使用，在一般情况下，我们可以使用register_chrdev()，因为它方便易用，但是在资源受限的情况下，我们可以直接使用cdev机制来实现。  
不管怎样，了解它底层的运行原理总是有必要的。  

好了，关于linux驱动程序-创建设备节点的另一种写法 就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.

