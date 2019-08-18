# 内核Kbuild详解系列(2) - 将第三方模块编译进内核


## 先从模块说起

通常来说，在驱动模块的开发阶段，一般是将模块编译成.ko文件，再使用
```
sudo insmod module.ko
```

或者

```
depmod -a
modprobe module
```

将模块加载到内核，相对而言，modprobe 要比 insmod 更加智能，它会检查并自动处理模块的依赖，而 insmod 出现依赖问题时仅仅是告诉你安装失败，自己想办法吧。  

*** 

## 将第三方模块编译进内核
这一章节我们并不关注模块的运行时加载，我们要讨论的是将第三方模块编译进内核，第三方模块，也就是那些本来不存在于 linux 内核中的模块，可能由我们自己编写，也可能是其它地方下载的。    

在上一章学习内核的 Makefile 规则的时候就知道，将驱动程序编译成模块时，只需要使用：
```
obj-m += module.o
```
指定相应的源代码(源代码为module.c)即可，所以就简单地得出结论：如果要将一个第三方模块编译进内核，只要执行下面的的指令就可以了：
```
obj-y += module.o
```
事实上，对于本身存在于linux源代码中的模块，这样是可以的。  

但是如果是第三方模块，这样是完全行不通的，要明白怎么将第三方驱动程序编译进内核，我们还是得先了解 linux 源码的编译规则，这里主要涉及到 linux 内核编译的配置。   

***  

## 问题的提出
在前文中，我们有提到，在编译 linux 内核之前需要对 linux 内核进行配置，配置的目的主要是确定整个内核源码中哪些需要编译进内核，而哪些编译成外部可加载模块，而哪些暂时不用管。  

通常，我们使用make menuconfig指令，那么，问题来了：

* make menuconfig 的模块列表是怎么来的？  
* 同时，我们应该怎样将驱动的源码C文件编译进内核呢？

要解答上述两个问题，这得从Makefile的执行流程说起.

***  


## 驱动程序编译进内核

### make的执行
首先，如果你有基本的 linux 内核编译经验，就知道在编译 linux 源码前，需要进行config(配置)，以决定哪些部分编译进内核、哪些部分编译成模块。  

通常使用make menuconfig，不同的config方式通常只是选择界面的不同，其中稍微特殊的 make oldconfig 则是沿用之前的配置。

在配置完成之后将生成一个 .config 文件，Kbuild编译系统根据 .config 文件选择性地进入子目录中执行编译工作。  

流程基本如上所说，但是我们需要知道更多的细节：
* make menuconfig 做了一些什么事？
* 顶层 Makefile 是怎样执行子目录中的编译工作的？

答案是这样的：

* make menuconfig 肯定是读取某个配置文件来陈列出所有的配置选项。经过一番寻找发现，几乎每个子目录中都有一个名为 Kconfig 的文件，这个文件负责配置驱动在配置菜单中的显示以及配置行为，且 Kconfig 遵循某种语法，make menuconfig 就是读取这些文件来显示配置项。  
* 递归地进入到每个子目录中，发现其中都有一个 Makefile ，可以想到，Makefile 递归地进入子目录，然后通过调用子目录中的 Makefile 来执行各级子目录的编译，最后统一链接。  

整个linux内核的编译都是采用一种分布式的思想，每个模块都负责处理自己这一部分的配置和编译，这样有利于裁剪和移植。  

***  

### 将模块添加到 config 列表中  
如果我们要将本来就存在于源码树中的模块编译进内核时，需要做的仅仅是在 make menuconfig 中将对应的模块选择为 y，再执行编译就可以了。  

但是，如果我们需要将一个第三方的模块编译进内核，首先需要解决的问题是：先把这个模块添加到配置列表中，使得执行 make menuconfig 时可以对其进行配置。  

所以，当我们添加一个驱动到模块中，我们需要做的事情就是：

1. 将需要编译的模块文件(夹)放到内核源码目录中，并配置Kconfig文件，使得在使用 make menuconfig 进行配置的时候可以对该模块进行配置。
2. 使用make menuconfig指令，将需要编译的内核模块选成Y。
3. 进行编译工作。


具体的步骤是这样的：

1. 将驱动源文件放在内核对应目录中，一般的驱动文件放在 drivers 目录下，字符设备放在 drivers/char 中，块设备就放在 drivers/blok 中，文件的位置遵循这个规律，如果是单个的字符设备源文件，就直接放在 drivers/char 目录下，如果内容较多足以构成一个模块，则新建一个文件夹。  

2. 如果是新建文件夹，因为是分布式编译，需要在文件夹下添加一个 Makefile 文件和 Kconfig 文件并修改成指定格式，如果是单个文件直接添加，并修改当前目录下的 Makefile 和 Kconfig 文件将其添加进去即可。   

3. 如果是新建文件夹，需要修改上级目录的 Makefile 和 Kconfig，以将文件夹添加到整个源码编译树中。  

4. 执行make menuconfig，如果文件(夹)放置位置和Kconfig配置没有出错，将在make menuconfig的配置界面中看到对应的配置项，选中，然后执行make

5. 将生成的新镜像以及相应boot文件拷贝到目标主机中，测试。

****  

## Kconfig的语法简述
上文中提到，在添加源码时，一般会需要一个 Kconfig 文件，这样就可以在 make menuconfig 时对其进行配置选择，在对一个源文件进行描述时，遵循相应的语法。  

在这里介绍一些常用的语法选项：

#### source
source:相当于C语言中的include，表示包含并引用其他Kconfig文件

***  

#### config
新建一个条目，用法：

```
source drivers/xxx/Kconfig
config TEST
    bool "item name"
    depends on NET
    select NET
    help
    just for test
```

config是最常用的关键词了，它负责新建一个条目，对应 linux 中的模块，条目前带有选项，例如：  

```
config TEST
```
config 后面跟的标识会被当成名称写入到 .config 文件中，比如：当此项被选择为[y]，即编译进内核时，最后会在 .config 文件中添加这样一个条目：  
```
CONFIG_TEST=y
```
CONFIG_TEST变量被传递给Makefile进行编译工作。  

下面是对于整个 config 语法中的各字段含义：

* bool "item name"：

    其中bool表示选项支持的种类，bool表示两种，编译进内核或者是忽略，还有另一种选项就是tristate，它更常用，表示支持三种配置选项：编译进内核、编译成可加载模块、忽略。而item name就是显示在menu中的名称。  

* depends on:

    表示当前模块需要依赖另一个选项，如果另一个选项没有没选择编译，当前条目选项不会出现在窗口中  

* select： 

    同样是依赖相关的选项，表示当前选项需要另外其他选项的支持，如果选择了当前选项，那么需要支持的那些选项就会被强制选择编译。  

* help：

    允许添加一些提示信息

****  

#### menu/menuend
用法：

        menu "Test option"
        ...
        endmenu
这一对关键词创建一个选项目录,该选项目录不能被配置，选项目录中可以包含多个选项

***  

#### menuconfig

相当于menu+config，此选项创建一个选项目录，而且当前选项目录可配置。  

***  
***  

## 实操示例
梳理了 Kbuild 添加的流程，接下来就以一个具体的示例来进行详细的说明。  

背景如下：
* 目标开发板为开源平台beagle bone，基于4.14.79内核版本，arm平台
* 需要添加的源代码为字符设备驱动，名为 cdev_test.c,新建一个目录 cdev_test
* 字符设备驱动实现的功能：在 /dev 目录下生成一个 basic_demo 文件，用来检测是否成功将源代码编译进内核

### 将驱动编译进内核


#### 放置目录
鉴于是字符设备，所以将源文件目录放置在 $KERNEL_ROOT/drivers/char/ 下面。  

如果是块设备，就会被放置在 block 下面，但是这并不是绝对的，类似USB为字符设备，但是独立了一个文件出来。

放置后目标文件位置为：$KERNEL_ROOT/drivers/char/cdev_test

***  

#### Kconfig文件
在$KERNEL_ROOT/drivers/char/cdev_test目录下创建一个Kconfig文件，并修改文件如下：
```
menu "cdev test dir"
config CDEV_TEST
        bool "cdev test support"
        default n
        help
        just for test ,hehe
endmenu
```

根据上文中对Kconfig文件的语法描述，可以看出，这个Kconfig文件的作用就是：
1. 在menuconfig的菜单中，在Device Driver(对应drivers目录) ---> Character devices(对应char目录)菜单下创建一个名为"cdev test dir"的菜单选项，  
        执行效果是这样的  
        ![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/linux_driver/compile_module_to_kernel/create_dir.png)  
2. 在"cdev test dir"的菜单选项下创建一个"cdev test support"的条目，这个条目的选项只有两个，[*]表示编译进内核和[]表示不进行编译，默认选择n，不进行编译。    
        执行效果是这样的  
        ![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/linux_driver/compile_module_to_kernel/create_item.png)  
3. 选择help可以查看相应的提示信息。  


在上文中还提到，Kconfig 分布式地存在于子目录下，同时需要注意的是，在编译时，配置工具并非无差别地进入每个子目录，收集所有的 Kconfig 信息，而是遵循一定的规则递归进入。  

那么，既然是新建的目录，怎么让编译器知道要进入到这个子目录下呢？  

答案是，在上级目录的 Kconfig 中包含当前路径下的 Kconfig 文件。  

打开 drivers/char/ 目录下的 Kconfig 文件，并且在文件的靠后位置添加：

```
source "drivers/char/xillybus/Kconfig"
```

就把新的Kconfig文件包含到系统中检索目录中了，那么 drivers/char/ 又是怎么被检索到的呢？   

就是在 drivers 的 Kconfig 中添加 drivers/char/ 目录下的 Kconfig 索引，以此类推。  

*** 

#### Makefile文件
在 $KERNEL_ROOT/drivers/char/cdev_test 目录下创建一个 Makefile 文件，并且编译 Makefile 文件如下：
```
obj-$(CONFIG_CDEV_TEST) += cdev_test.o
```
表示当前子目录下 Makefile 的作用就是将 cdev_test.c 源文件编译成 cdev_test.o 目标文件。  

值得注意的是，这里的编译选项中使用的是：
```  
obj-$(CONFIG_CDEV_TEST)
```  
而非
```
obj-y
```

如果确定要将驱动程序编译进内核永远不变，那么可以直接写死，使用 obj-y ，如果需要进行灵活的定制，还是需要选择第一种做法。  

CONFIG_CDEV_TEST 是怎么被配置的呢？在上文提到的 Kconfig 文件编写时，有这么一行：
```
config CDEV_TEST
...
```

在Kconfig被添加到配置菜单中，且被选中编译进内核时，就会在 .config 文件中添加一个变量：
```
CONFIG_CDEV_TEST=y
```
自动添加 CONFIG_ 前缀，而名称 CDEV_TEST 则是由 Kconfig 指定，看到这里，我想你应该明白了这是怎么回事了。  

是不是这样就已经将当前子目录添加到内核编译树中了呢？  

其实并没有，就像Kconfig一样，Makefile 也是分布式存在于整个源码树中，顶层 Makefile 根据配置递归地进入到子目录中，调用子目录中的 Makefile 进行编译。     

同样地，需要修改 drivers/char/ 目录下的 Makefile 文件，添加一行：
```
obj-$(CONFIG_CDEV_TEST)         += cdev_test/
```
在编译时，如果 CONFIG_CDEV_TEST 变量为y，cdev_test/Makefile 就会被调用。  

***  

#### 在make menuconfig中选中
生成配置的部分完成，就需要在menuconfig菜单中进行配置，执行：
```
make menuconfig
```
进入目录选项 Device Driver --> Character devices--->cdev test dir.

然后按'y'选中模块 cdev test support

保存退出，然后执行编译：
```
make
```

*** 

#### 更换目标开发板上镜像
将 vmlinuz(zImage)、System.map 等 /boot/ 下的目标文件拷贝到目标主机的 /boot 目录下。    

在编译生成的modules拷贝到目标主机的 /lib/modules 目录下。  

需要注意的是：启动文件也好，模块也好，在目标板上很可能文件名为诸如vmlinuz-$version，会包含版本信息，需要将文件名修改成一致，不然无法启动。对于模块而言，就是相应模块无法加载。  


#### 验证
最后一步就是验证自己的驱动程序是否被编译进内核，如果被编译进内核，驱动程序中的module_init()程序将被系统调用，完成一些开发者指定的操作。  

这一部分的验证操作就是各显身手了。  

**注意：更换目标开发板上镜像和验证部分是与硬件强相关的操作，各大平台的操作可能有些许差别，原理都是更新/boot 下的启动文件。**

****  


## 小结
将一个第三方模块编译进内核的步骤主要是：
* 将模块文件(夹)添加到内核源码对应文件夹下，并配置Kfoncif文件
* 使用make menuconfig 将其配置成Y
* 编译

这一章节主要以将模块编译进内核为线索，介绍了linux内核中的模块配置以及如何将第三方模块编译进内核。

***  

好了，关于 linux将三方驱动程序编译进内核 的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.

