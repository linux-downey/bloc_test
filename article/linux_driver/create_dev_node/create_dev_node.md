# linux字符设备驱动程序--创建设备节点
***基于4.14内核,运行在beagleBone green***

在上一讲中，我们写了第一个linux设备驱动程序——hello_world,在驱动程序中，我们什么也没有做，仅仅是打印了两条日志消息，今天，我们就要丰富这个设备驱动程序，在/dev目录下创建一个设备节点，用户通过读写文件来与内核进行交互。  

## 预备知识
在linux中，一切皆文件，不管用户是控制某个外设又或者是操作I/O，都是通过文件实现。 

设备驱动程序被装载在内核中运行，当用户程序需要使用对应设备时，自然不可能直接访问内核空间，那么用户程序应该怎么做呢？  

答案是内核将设备驱动程序操作接口以文件接口的形式导出到用户空间，一般为相应的设备在/dev目录下建立相应的操作接口文件，自linux2.6内核版本以来，内核还会在系统启动时创建sysfs文件系统，内核同样可以将设备操作接口导出到/sys目录下。  

举个例子：在开发一款温度传感器时，内核驱动模块可以在驱动程序中实现传感器的初始化，然后在/dev目录下创建对应文件，关联/dev的读写回调函数，当用户访问/dev下相应文件时，就会调用相应的回调函数，执行设备的操作。   

下面我们就演示如何在/dev目录下创建一个设备节点。  

## 程序实现
```C  
#include <linux/init.h>  
#include <linux/module.h>
#include <linux/device.h>  
#include <linux/kernel.h>  
#include <linux/fs.h>
#include <linux/uaccess.h>

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


static int __init basic_init(void)
{
    printk(KERN_ALERT "Driver init\r\n");
    /*注册一个新的字符设备，返回主设备号*/
    majorNumber = register_chrdev(0,DEVICE_NAME,&file_oprts);
    if(majorNumber < 0 ){
        printk(KERN_ALERT "Register failed!!\r\n");
        return majorNumber;
    }
    printk(KERN_ALERT "Registe success,major number is %d\r\n",majorNumber);

    /*以CLASS_NAME创建一个class结构，这个动作将会在/sys/class目录创建一个名为CLASS_NAME的目录*/
    basic_class = class_create(THIS_MODULE,CLASS_NAME);
    if(IS_ERR(basic_class))
    {
        unregister_chrdev(majorNumber,DEVICE_NAME);
        return PTR_ERR(basic_class);
    }

    /*以DEVICE_NAME为名，参考/sys/class/CLASS_NAME在/dev目录下创建一个设备：/dev/DEVICE_NAME*/
    basic_device = device_create(basic_class,NULL,MKDEV(majorNumber,0),NULL,DEVICE_NAME);
    if(IS_ERR(basic_device))
    {
        class_destroy(basic_class);
        unregister_chrdev(majorNumber,DEVICE_NAME);
        return PTR_ERR(basic_device);
    }
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
        printk(KERN_INFO "Send file!!");
        return 0;
    }
    else{
        printk(KERN_ALERT "ERROR occur when reading!!");
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
        printk(KERN_INFO "Recieve file!!");
    }
    else{
        printk(KERN_ALERT "ERROR occur when writing!!");
        return -EFAULT;
    }
    printk(KERN_INFO "Recive data ,len = %s",recv_msg);
    return len;
}

/*当用户打开设备文件时，调用这个函数*/
static int basic_release(struct inode *node,struct file *file)
{
    printk(KERN_INFO "Release!!");
    return 0;
}

/*销毁注册的所有资源，卸载模块，这是保持linux内核稳定的重要一步*/
static void __exit basic_exit(void)
{
    device_destroy(basic_class,MKDEV(majorNumber,0));
    class_unregister(basic_class);
    class_destroy(basic_class);
    unregister_chrdev(majorNumber,DEVICE_NAME);
}

module_init(basic_init);
module_exit(basic_exit);
```
### 程序注解
看程序当然是要从入口函数开始，我们将目光投入到basic_init函数：
1. majorNumber = register_chrdev(0,DEVICE_NAME,&file_oprts);调用这个函数创建了一个字符设备。
    * 参数1为次设备号，在linux内核中，一个设备由主次设备号标记。
    * 参数2为设备名称，这个设备名称将会作为/dev下建立的设备
    * 参数3为绑定的file operation结构体，当用户空间读写操作设备时，产生系统调用，即调用这个结构体中相应的读写函数
    * 返回主设备号
2. basic_class = class_create(THIS_MODULE,CLASS_NAME);调用这个函数创建一个class，同时在/sys/class目录下创建一个目录，作为当前设备的描述信息。  

    * 参数1指定当前module,主要是用来标识当这个模块正被操作时阻止模块被卸载。
    * 参数2指定class名，这个名称将作为/sys/class下的目录名  

3. basic_device = device_create(basic_class,NULL,MKDEV(majorNumber,0),NULL,DEVICE_NAME);调用这个函数在/dev下注册一个用户空间设备:/dev/DEVICE_NAME
    * 参数1为传入的class信息
    * 参数2为父目录，这里为NULL，表示默认直接挂在/dev下
    * 参数3为设备号，由主次设备构成的设备号
    * 参数4为drvdata指针，指向设备数据
    * 参数5为*fmt，与printf函数类型，支持可变参数，表示设备节点的名称

### 执行原理
驱动使用register_chrdev()函数在内核中注册一个设备节点，同时将初始化的file_operation结构体注册进去，内核会维护一个file_operation结构体集合，注册一个file_operation结构体并且返回设备号，在这里将设备号和结构体相关联。  

在用户空间使用device_create()在/dev目录下创建新的设备节点，但是在这个目录创建时并没有关联相应的file_operation结构体，那我们在对设备节点进行read，write操作时，是怎么调用相应的file_operation结构体中的接口的呢？  

答案是通过设备号，内核维护file_operation结构体数组，并且将其与设备号进行关联，另一方面，在/dev下创建设备节点时，将设备节点与设备号进行关联。  

所以在操作设备节点时，可以得到设备节点关联的设备号，再通过设备节点找到相应的file_operation结构体，再调用结构体中相应的函数，执行完毕返回到用户空间。  

所以当模块加载完成后，整个过程是这样的：

    用户打开/dev/DEVICE_NAME设备产生系统调用 
        ->系统找到设备节点对应的设备号 
            ->通过设备号找到内核维护的file_operation结构体集合中对应的结构体  
                ->由于初始化时指定了.open = basic_open，调用basic_open函数 
    返回到用户空间
    用户的下一步操作...  

需要注意的是，用户的read会最终会触发调用file_operation结构体中的相应read函数，前提是我们进行了相应的赋值：.read = basic_read,但是用户的read函数的返回值并非就是file_operation结构体中basic_read的返回值，从用户read /dev下的文件到触发调用file_operation结构体中.read还有一些中间过程。  

### copy_to_user()和copy_from_user()
linux kernel是操作系统的核心，掌握着整个系统的运行和硬件资源的分配。  

所以为了安全考虑，linux的内存空间被划分为用户空间和内核空间，而如果用户空间需要使用到内某些由核掌控的资源，就必须提出申请，这个申请就是产生系统调用，遵循一些内核指定的接口来访问内核资源。  

将用户空间和内核空间进行隔离能够保障内核的安全，因为用户进程的任何行为都由内核最终把控，出现问题就直接结束进程。而内核一旦出现问题，很大可能直接导致死机或者产生一些不可预期的行为，这是我们不愿意看到的。  

既然进行了隔离，那么用户进程和内核之间的数据交换就得通过专门的接口而非随意的指针相互访问，这两个接口就是copy_to_user()和copy_from_user()。  

顾名思义copy_to_user()就是将内核数据copy到用户空间，copy_from_user()就是将用户数据copy到内核中，这两种行为都由内核管理。  

这两个接口主要做了两件事：
1. 检查数据指针是否越界，这对内核安全来说是非常重要的，用户程序永远不可能直接访问内核空间
2. copy数据

### 编译加载
编译依旧延续上一章节的操作，修改Makefile：

     obj-m+=create_device_node.o
    all:
            make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
    clean:
            make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
编译完成之后加载模块：

    sudo insmod create_dev_node.ko 

然后使用lsmod命令进行检查，如果一切正常，我们就可以在/dev目录下看到我们新创建的设备节点:/dev/$DEVICE_NAME。  

## 用户空间的操作代码
加载内核模块成功之后，接下来的事情就是在用户空间操作设备节点了，我们尝试着打开设备节点，然后对其进行读写：  

create_device_node_user.c:
```C
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static char buf[256] = {1};
int main(int argc,char *argv[])
{
    int fd = open("/dev/basic_demo",O_RDWR);
    if(fd < 0)
    {
        perror("Open file failed!!!\r\n");
    }
    int ret = write(fd,"huangdao!",strlen("huangdao!"));
    if(ret < 0){
        perror("Failed to write!!");
    }
    ret = read(fd,buf,7);
    if(ret < 0){
        perror("Read failed!!");
    }
    else
    {
        printf("recv data = %s \n",buf);
    }
    close(fd);
    return 0;
}
```
在上面的代码示例中，创建了/dev/basic_demo这个设备节点，在user程序中，先打开/dev/basic_demo设备，然后写字符串"huangdao",写完之后然后读取7个字节。  

在上面的设备程序中实现：read将调用basic_read()函数，而write将调用basic_write()函数，在basic_write()函数中，接收用户空间的数据并打印出来，而在basic_read()中，将msg中的信息返回到用户空间。  

### 对用户程序编译运行
编译很简单：

    gcc create_device_node_user.c -o user
运行：

    sudo ./user

内核端的log输出：
    Dec 20 14:12:55 beaglebone kernel: [  433.070666] Open file
    Dec 20 14:12:55 beaglebone kernel: [  433.073308] Recieve file!!
    Dec 20 14:12:55 beaglebone kernel: [  433.073318] Recive data ,len = huangdao!
    Dec 20 14:12:55 beaglebone kernel: [  433.073335] Send file!!

用户端的log输出：

    recv data = Downey!


## linux设备节点访问权限
在加载完上述设备驱动程序之后，我们可以看到生成了一个新设备/dev/basic_demo,查看这个设备节点的权限：

    ls -l /dev/basic_demo
输出：

    crw------- 1 root root 241, 0 Dec 20 14:10 /dev/basic_demo
发现权限是只有root用户才能读写，所以在上面执行用户程序访问设备节点时，我们必须加上sudo以超级用户执行程序，既然设备驱动程序的节点服务于普通用户，那么普通用户如果没有权限访问，那岂不是白瞎。所以我们要修改设备节点的属性。  

第一个办法，当然是最简单粗暴的,使用root权限直接修改：

    sudo chmod 666 /dev/basic_demo
这种办法是可以完成修改的，而且也达到了用户可访问的目的，优点就是简单。  

但是可别忘了，内核模块具有可动态加载卸载的属性，如果我们每一次加载模块之后都要以root权限重新去设置一次设备节点权限，在linux严格的权限管理系统下，在某些场景这并不合适。  

第二个办法：使用系统提供的方式：
* 首先，我们可以使用udevadm info -a -p /sys/class/$CLASS_NAME/$DEVICE_NAME来查看模块信息:

    udevadm info -a -p /sys/class/basic_class/basic_demo
输出：
    looking at device '/devices/virtual/basic_class/basic_demo':
    KERNEL=="basic_demo"
    SUBSYSTEM=="basic_class"
    DRIVER==""
在这里可以看到模块的设备名(KERNEL)，class名称(SUBSYSTEM)，这里只是一个查询作用，获取相应信息，如果能记住就可以省略这一步骤。

    tips:
    在上述的驱动程序中，我们使用class_create()创建了一个设备节点，在/sys/class目录下生成了相应的以$CLASS_NAME为名称的目录，这个目录下存放着模块信息。

* 然后，在/etc/udev目录下创建一个.rules为后缀的文件，这个文件就是udev的规则文件，可对权限进行管理:
    * 文件名以数字开头，因为这个目录下的文件都以数字开头，在不了解全部原理之前我们得遵循系统的规则，事实上不以数字开头也没有问题，文件必须以.rules结尾，这里创建的文件名为：99-basic_demo.rules.
    * 填充文件名：

        KERNEL=="basic_demo", SUBSYSTEM=="basic_class", MODE="0666"
完成以上操作，再加载模块时，我们就可以查看设备节点信息了：

    ls -l /dev/basic_demo
结果显示：

    crw-rw-rw- 1 root root 241, 0 Dec 20 14:37 /dev/basic_demo
表示权限修改完成，经过这次修改，每次加载完模块，生成的设备节点文件都是.rules文件中指定的访问权限了。


关于/etc/udev/下.rules文件规则请参考![官方文档](http://reactivated.net/writing_udev_rules.html)    


好了，关于linux设备驱动中创建设备节点的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.
