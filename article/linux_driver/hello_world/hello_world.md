# linux字符设备驱动程序--hello_world

***基于4.14内核,运行在beagleBone green***


## PC端的设备驱动程序
有过电脑使用经验的人都知道，当我们将外部硬件设备比如鼠标键盘插入到电脑端口(通常是USB口)时，在windows系统右下角会弹出"安装设备驱动程序"的显示框，那么，为什么每个硬件都需要安装设备驱动程序才能使用呢？  

首先，每个硬件都有相应的功能，鼠标的功能就是将鼠标的位移与点击状态转换成相应的数据，然后将数据传输给电脑，然后电脑根据收到的数据移动屏幕上的光标。  

如果没有相应的鼠标驱动程序，电脑并不知道鼠标的接口以什么协议将数据传输过来，也不知道怎么解析相应的数据，所以当然电脑上的光标不会跟随鼠标的移动而移动，归根结底，鼠标的移动和电脑上光标的移动是两者间数据同步的结果。  

同理，打印机也是一样，电脑将文件数据以某种格式传递给打印机，然后通过控制数据控制打印机的运行，打印机驱动程序基本上也是识别接收数据以及对数据的处理，这就是为什么一般外部设备都需要使用一根数据线与主机进行连接。  


## MCU中设备驱动程序
在基于MCU的普通嵌入式驱动程序开发中，并不会经常接触到鼠标、键盘、硬盘这一类的设备，多数是一些较为简单的传感器设备、小容量的存储设备等等，通常数据的传输使用的是spi、i2c、串口这一类的串行通信协议,通常一个设备驱动程序的开发就是这样的流程：
* 数据传输层，一般在MCU上集成相应的硬件控制器，配置寄存器即可
* 数据处理层，根据收发的数据对数据进行解析，然后控制设备做相应处理。  



## linux设备驱动程序
在linux系统中，一个硬件设备想要运行同样需要提供设备驱动程序，底层的原理和MCU中的设备驱动程序一样：收发数据以及处理数据，只是由于桌面操作系统的特殊性，设备驱动程序的流程会复杂很多。  

在这一系列的文章中，我将介绍怎么去编写linux设备驱动程序，linux内核支持将设备驱动程序编译进内核和运行时加载进内核两种方式，在这一系列文章中，主要讨论编译可加载模块,一般开发者会将设备驱动程序编译成可加载模块，在linux运行时动态加载进内核，以实现对应设备的驱动。   

linux将内核与用户分离，驱动模块运行在内核空间中，而应用程序运行在用户空间，内核主要对公共且有限的资源进行管理、调度，比如硬件外设资源、内存资源等。  

当用户需要使用系统资源时，通过系统调用进入内核，由内核基于某种调度算法对这部分资源进行调度。  

在用户空间看来，每个用户进程都请求到了相应的资源得以运行。  

在内核空间看来，避免用户程序与有限的硬件资源直接交互，保护了相应资源，且用户程序之间相互隔离，互不影响，用户程序的崩溃并不会影响内核空间，保障了系统的稳定运行。  

但是，linux设备驱动程序的开发目的是将模块加载到内核空间中，内核空间的操作向来都是危险的，一旦不小心就很可能导致内核的崩溃，同时我们需要掌握一些内核调试的技巧。  

## 实现一个内核驱动程序的hello_world

### 程序实现
话不多说，先来个hello_world吧。  
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


上述代码就是一个设备驱动程序代码框架，这套框架主要的任务就是将内核模块中的init函数动态地注册到系统中并运行，由module_init()和module_exit()来实现，分别对应驱动的加载和卸载。  

只是它并不做什么事，仅仅是打印两条语句而已，如果要实现某些驱动，我们就可以在init函数中进行相应的编程。     


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

**针对编译模块makefile的介绍可以看看我介绍makefile的博客：**  
[linux可加载模块makefile](https://www.cnblogs.com/downey-blog/p/10486907.html)  
[linux 内核makefile总览](https://www.cnblogs.com/downey-blog/p/10486863.html) 

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

### 与上一版本的区别
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

### sysfs
sysfs是一个文件系统，但是它并不存在于非易失性存储器上(也就是我们常说的硬盘、flash等掉电不丢失数据的存储器)，而是由linux系统构建在内存中，简单来说这个文件系统将内核驱动信息展现给用户。  

当我们装载hello_world_PLUS.ko时，会在/sys/module/目录下生成一个与模块同名的目录即hello_world_PLUS,目录里囊括了驱动程序的大部分信息，查看目录：

    ls /sys/module/hello_world_PLUS
输出：

    coresize  initsize   notes       refcnt    srcversion  uevent
    holders   initstate  parameters  sections  taint       version

这一部分的知识仅仅是在这里引出提一下，建立个映象，在这里就不再赘述，之后博主会更新相关的博客。  


好了，关于linux驱动程序-hello_world就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.

