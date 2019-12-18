# 字符设备的驱动访问流程

在linux的设计哲学中，"一切皆文件"这个概念充斥在linux实现的各个角落，对于我们驱动工程师而言，自然避免不了与文件打交道。  

作为一名linux驱动研究爱好者，对文件系统的了解并没有多深入，所以本篇博客的目的并非是解析linux的文件系统，博主也没有那个能力。  

## 设备驱动接口

既然在linux下做驱动工作，通常接触到的是设备文件，开发者提供设备驱动，同时提供用户接口，在用户使用这些接口时，与内核驱动进行交互以实现相应的功能。不知道作为开发者的你是否有过这样的疑惑：用户文件接口到底是怎么调用到我们的驱动程序的？

设备文件的导出有多种，传统的方式可以将设备文件导出到 /dev(驱动接口) 和 /proc(驱动信息) 目录下，后来linux专门扩展了驱动程序相关的文件系统，sysfs 、 debugfs等作为设备导出和调试，sysfs被挂载到 /sys 目录下，专门负责记录驱动文件。而 debugfs 专门用作调试使用，被挂载到 /sys/kernel/debug 目录下。  

至于为什么一个驱动部分要对应这么多的系统组件？

对于其他的模块而言，比如文件系统、内存管理、系统调度这些部分，虽然会有更新，但通常是进行优化，增量更新的频率低到可以忽略不计。

而对于驱动设备，情况就完全不一样了，linux作为宏内核，通常是将大量的设备驱动集成到内核中，每对应一个新的硬件，很可能就代表着一类新的驱动程序需要添加到内核，而现在硬件的更新换代速度是非常快的，所以在整个内核代码中，驱动代码所占的比重飞速增加，而且在可见的未来这个增长趋势并不会放缓。  
就像一个老板可以管理20个员工的公司，但是增长到100个人，一个人管理起来就会力不从心，从而导致混乱。比较好的做法是将员工以技术、业务、财务等工种进行分类管理，才能避免员工职能的混淆。  

在传统的linux内核中，设备驱动接口被导出到 /dev 目录下，而设备驱动的描述信息被放在 /proc 目录下，/proc 目录提供在运行时访问内核内部数据结构、改变内核设置的机制，内核驱动放到这里也未尝不可。  

但是随着驱动程序的进一步增长，驱动模块在内核中占据的比重越来越大，/proc 目录下的驱动部分越来越多，也越来越臃肿。孩子长大了自然得分家，sysfs 也就在 2.6 版本中应运而生，专门针对linux中内核驱动的管理。  

本章节只讨论驱动涉及到的文件系统，而 /dev /proc /sys 都是基于 ram 的文件系统，也就是在系统启动之后被生成并加载到 ram 中，而不是直接存储在磁盘上。

为了便于理解，博主尽可能地不引入文件系统中过多的框架，而专注于理解设备接口文件的调用流程，因为整个文件系统的框架并不简单，事实上我们只需要了解其中一部分。当然，如果你是一个热爱技术的人，自然是有必要先去了解文件系统。

## 驱动

### 驱动接口的提供

在编写字符设备的驱动程序时，我们通常需要这样的操作：

```
//文件操作回调函数，对应用户操作 open，read
struct file_operations ops{
    .open = drv_open;
    .read = drv_read;
};

//设备号的注册
//使用系统分配设备号
//alloc_chrdev_region(&devid,base_min,cnt,"drv_name")

//使用确定的设备号
devid = MKDEV(major, minor);
register_chrdev_region(devid, MAX_PINS, "drv_name");

//cdev的注册
struct cdev *cdev = cdev_alloc();
cdev_init(cdev,&ops);
cdev_add(cdev);

//创建device
class = class_create();
device_create();

```

其中包含的操作有：文件操作接口的回调函数、设备号的注册绑定以及cdev的注册。

#### 设备号
就像每个人都有自己的身份证，设备在 linux 中的标识就是设备号，每个设备号唯一标识一个具体的设备。  

内核中，设备号由专用类型 dev_t 表示，它的定义在 include/linux/types.h 中：

```
typedef __u32 __kernel_dev_t;
typedef __kernel_dev_t		dev_t;
```

与平台无关，设备号恒为32位变量，分为主设备号和次设备号。  

主设备号代表的是一类设备，而次设备号则代表一类设备中的多个设备，比如 mmc 表示一类设备，所有mmc使用同一个主设备号，而不同的 mmc 设备在共用该主设备号的前提下以次设备号进行区分。  

比如在我的 beaglebone 开发板上，使用以下指令来验证设备号：

```
ls -l /dev/mmcblk*
```
输出为：

```
brw-rw---- 1 root disk 179,  0 Dec  1 06:44 /dev/mmcblk0
brw-rw---- 1 root disk 179,  1 Dec  1 06:44 /dev/mmcblk0p1
brw-rw---- 1 root disk 179,  8 Dec  1 06:44 /dev/mmcblk1
```

该开发板上分别有 emmc 和 sd 卡，共用主设备号 179，而使用不同的设备号如：0,1,8。  

同时，你也可以使用以下指令来查看系统中的设备号使用情况：

```
cat /proc/devices
```

既然是主次设备号共用一个 dev_t 类型变量来描述，自然主次设备号需要区分开来，在目前的版本中(4.x),其中前 12 位表示主设备号，而后 20 位表示次设备号。但是，我们不应该假定这是一个标准规则，写出这样的代码：

```
major = dev >> 20;
minor = dev & 0xffff;
```
而应该使用内核定义在 include/linux/kdev.h 中的接口来实现：

```
#define MINORBITS	20
#define MINORMASK	((1U << MINORBITS) - 1)

#define MAJOR(dev)	((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev)	((unsigned int) ((dev) & MINORMASK))
#define MKDEV(ma,mi)	(((ma) << MINORBITS) | (mi))
```

不论是从主次设备号构建 dev_t 类型设备号，还是通过 dev_t 类型获取主次设备号，都使用内核提供的标准接口，一方面我们应该养成这样的习惯，另一方面是随着linux的演变，设备号的表达形式可能会发生变化,比如扩展到64位，使用标准宏有利于程序的向后兼容，而不会随着版本更新而产生兼容问题。  


### 设备号的申请和添加
设备号作为一种由内核管理的资源，自然是需要向内核进行申请的，一般来说，系统自带的设备将会占用一些设备号，使用 cat /proc/devices 可以查看系统已占用的设备号。  

在编写自己的设备驱动程序时，同样需要使用设备号，设备号可以由开发者自己指定，同样也可以向系统申请，通常，在创建一个新类型的设备时，都是向系统申请，这样避免了认为指定造成的设备号之间的冲突。

人为指定并注册设备号接口：

```
devid = MKDEV(major, minor);
register_chrdev_region(devid, MAX_PINS, "drv_name");
```

系统申请设备号接口：

```
alloc_chrdev_region(&devid,base_min,cnt,"drv_name");
```
申请到的设备号被保存在 devid 中。

无论是人为指定还是系统申请，都需要将该设备号注册到系统中，其调用的接口是：
```
register_chrdev_region -> __register_chrdev_region()

alloc_chrdev_region -> __register_chrdev_region()
```

接下来看 __register_chrdev_region 的源代码：
```
static struct char_device_struct *
__register_chrdev_region(unsigned int major, unsigned int baseminor,
			   int minorct, const char *name){
    ...
    if (major == 0) {
		ret = find_dynamic_major();
		if (ret < 0) {
			pr_err("CHRDEV \"%s\" dynamic allocation region is full\n",
			       name);
			goto out;
		}
		major = ret;
	}

}

```





