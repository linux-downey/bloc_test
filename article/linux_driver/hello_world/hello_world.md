# linux字符设备驱动程序--hello_world

***基于4.14内核,运行在beagleBone green***

## 实现一个内核驱动程序的hello_world

### 程序实现
想要入手一项新技术，研究一套新框架，话不多说，先来个hello_world吧。  

hello_world.c:  
    
    #include <linux/init.h>             
    #include <linux/module.h>          
    #include <linux/kernel.h>   

    //指定license版本
    MODULE_LICENSE("GPL");              
    
    //设置初始化入口函数
    static int __init hello_world_init(void)
    {
        printk(KERN_DEBUG "hello world!!!\n");
        return 0;
    }
    
    //设置出口函数
    static void __exit hello_world_exit(void)
    {
        printk(KERN_DEBUG "goodbye world!!!\n");
    }
    
    //将上述定义的init()和exit()函数定义为模块入口/出口函数
    module_init(hello_world_init);
    module_exit(hello_world_exit);


上述代码就是一个设备驱动程序，只是它并不做什么事，仅仅是打印两条语句而已。   

### 编译
编译这个程序，我们都知道，linux下编译程序一般使用make工具(简单的程序可以直接命令行来操作)，以及一个Makefile文件，在内核开发中，Makefile并不像应用程序那样，而是经过了一些封装，我们只需要往其中添加需要编译的目标文件即可：

    obj-m+=hello_world.o
    all:
            make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
    clean:
            make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
其中hello_world.o就是目标文件，make工具会根据目标文件自动推导来编译hello_world.c文件。  
编译：

    make
编译结果会在当前目录生成hello_world.ko文件，这个文件就是我们需要的内核模块文件了。  

### 加载
编译生成了内核文件，接下来就要将其加载到内核中，linux支持动态地添加和删除模块，所以我们可以直接在系统中进行加载：

    sudo insmod hello_world.ko
我们可以通过lsmod命令来检查模块是否被成功加载：

    lsmod | grep "hello_world"
lsmod显示当前被加载的模块。  

同时，我们也可以卸载这个模块：

    sudo rmmod hello_world.ko
同样我们也可以通过lsmod指令来查看模块是否卸载成功。  

但是，在这里我们并没有看到有任何打印信息的输出，在程序中我们使用printk函数来打印信息。  

事实上，printk属于内核函数，它与printf在实现上唯一的区别就是printk可以通过指定消息等级来区分消息输出，在在这里，printk输出的消息被输出到/var/log/kern.log文件中，我们可以通过另开一个终端来查看内核日志消息：

    tail -f /var/log/kern.log
tail -f表示循环读取/var/log/kern.log文件中的消息并显示在当前终端中，这样我们就可以在终端查看内核中printk输出的消息。  

如果你不想重新开一个终端来显示内核日志，希望直接显示在当前终端，你可以这样做：

    tail -f /var/log/kern.log &
仅仅是将这条指令放在当前进程后台执行，当前终端关闭时，这个后台进程也会被关闭。  

我们使用开第二种方式来显示内核消息的方式来重新加载hello_world.ko模块：

    sudo insmod hello_world.ko


然后查看消息输出，果然，在终端输出日志：

    Dec 16 10:01:15 beaglebone kernel: [98355.403532] hello world!!!


然后卸载，同样，终端中显示相应的输出：

    Dec 16 10:01:50 beaglebone kernel: [98390.181631] goodbye world!!!

从结果可以看出，在模块加载时，执行了hello_world_init()函数，在卸载时执行了hello_world_exit()函数。


## hello_world PLUS版本
在上面实现了一个linux内核驱动程序(虽然什么也没干)，接下来我们再来添加一些小功能来丰富这个驱动程序：
* 添加模块信息  
* 模块加载时传递参数。  


废话不多说，直接上代码：
hello_world_PLUS.c:

    #include <linux/init.h>             
    #include <linux/module.h>          
    #include <linux/kernel.h>   

    MODULE_AUTHOR("Downey");      //作者信息
    MODULE_DESCRIPTION("Linux kernel driver - hello_world PLUS!");  //模块的描述，可以使用modinfo xxx.ko指令来查看
    MODULE_VERSION("0.1");             //模块版本号
    //指定license版本
    MODULE_LICENSE("GPL");              

    static char *name = "world";
    module_param(name,charp,S_IRUGO);                                 //设置加载时可传入的参数
    MODULE_PARM_DESC(name,"name,type: char *,permission: S_IRUGO");        //参数描述信息
    
    //设置初始化入口函数
    static int __init hello_world_init(void)
    {
        printk(KERN_DEBUG "hello %s!!!\n",name);
        return 0;
    }
    
    //设置出口函数
    static void __exit hello_world_exit(void)
    {
        printk(KERN_DEBUG "goodbye %s!!!\n",name);
    }
    
    //将上述定义的init()和exit()函数定义为模块入口/出口函数
    module_init(hello_world_init);
    module_exit(hello_world_exit);

### 在上一版本的区别
添加了MODULE_AUTHOR()，MODULE_DESCRIPTION()，MODULE_VERSION()等模块信息  

添加了module_param()传入参数功能  
### 编译
编译之前需要修改Makefile，将hello_world.o修改为hello_world_PLUS.o。  

### 加载
在上述程序中我们添加了module_param这一选项，module_param支持三个参数：变量名，类型，以及访问权限，我们可以先试一试传入参数：

    sudo insmod hello_world_PLUS.ko Downey
查看日志输出，显示：

    Dec 16 10:07:38 beaglebone kernel: [98738.153909] hello Downey!!!
看到模块中name变量被赋值为Downey，表明参数传入成功。
然后卸载：

    sudo insmod hello_world_PLUS
日志输出：

    Dec 16 10:08:12 beaglebone kernel: [98772.191127] goodbye Downey!!!

### 传入多个参数
上面讲解了传入一个参数时的示例，如果传入多个参数呢？该怎么修改，这个就留给大家去尝试了。

### 添加的模块信息
在hello_world_PLUS中，我们添加了一些模块信息，可以使用modinfo来查看：

    modinfo hello_world_PLUS.ko
输出：

    filename:       /home/debian/linux_driver_repo/hello_world_PLUS/hello_world_PLUS.ko
    license:        GPL
    version:        0.1
    description:    Linux kernel driver - hello_world PLUS!
    author:         Downey
    srcversion:     549C47CB670506CE16F56D8
    depends:
    name:           hello_world_PLUS
    vermagic:       4.14.71-ti-r80 SMP preempt mod_unload modversions ARMv7 p2v8
    parm:           name:type: char *,permission: S_IRUGO (charp)

### 关于sysfs
sysfs是一个文件系统，但是它并不存在于非易失性存储器上(也就是我们常说的硬盘、flash等掉电不丢失数据的存储器)，而是由linux系统构建在内存中，简单来说这个文件系统将内核驱动信息展现给用户。  
当我们装载hello_world_PLUS.ko时，会在/sys/module/目录下生成一个与模块同名的目录即hello_world_PLUS,目录里囊括了驱动程序的大部分信息，查看目录：

    ls /sys/module/hello_world_PLUS
输出：

    coresize  initsize   notes       refcnt    srcversion  uevent
    holders   initstate  parameters  sections  taint       version
这一部分的知识并不简单，在这里就不再赘述，之后博主会更新相关的博客。  

好了，关于linux驱动程序-hello_world就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.

