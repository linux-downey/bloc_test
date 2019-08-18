# linux Kbuild 详解系列(1)-外部模块的编译

在开发linux内核驱动时，免不了要接触内核编译相关的 Makefile 的编写和修改，尽管网上的 Makefile 模板一大堆，做一些简单的修改就能用到自己的项目上，但是，对于这些基础的东西，更应该做到知其然并知其所以然。  
 
linux内核使用的是kbuild编译系统，在编译可加载模块时，其 Makefile 的风格和常用的编译C程序的 Makefile 有所不同，尽管如此，Makefile 的作用总归是给编译器提供编译信息。

在参考[官方文档](https://github.com/torvalds/linux/blob/master/Documentation/kbuild/modules.txt)的基础上，我们来看看外部模块编译的那些操作。 

**** 

## 最简单的Makefile
我们先来看看一个最简单的Makefile是怎样的：

```
obj-m+=hello.o
all:
        make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
clean:
        make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
```

这个 Makefile 的作用就是编译 hello.c 文件，最终生成 hello.ko 文件。  

如果你对 Makefile 有一定的了解，那么可以发现，这并不是一个完整的 Makefile ，至少它不能独立运行。   

其实，这个 Makefile 借助于 linux 内核的 Kbuild 系统(专用于 linux 内核编译)进行编译，也就是内嵌到 Kbuild 系统进行单个模块的编译，到底是怎么回事，我们可以继续往下看。  

****  

### obj-m+=hello.o
obj-m ，该变量在内核 top Makefile 中被定义，它的值为一系列的可加载外部模块名，以 .o 为后缀。   

相对应的，obj-y 为一系列的编译进内核的模块名，以.o为后缀。   

可以看到，这里并没有输入 hello.c 源文件，熟悉 Makefile 的人应该知道，这得益于 Makefile 的自动推导功能，需要编译生成 filename.o 文件而没有显示地指定 filename.c 文件位置时， make 查找 filename.c 是否存在，如果存在就正常编译，如果不存在，则报错。  

obj-m+=hello.o，这条语句就是显式地将 hello.o 添加到待编译外部可加载模块列表中，编译成 hello.ko ,而 hello.o 则由 make 的自动推导功能编译 hello.c 文件生成。  

**** 

### all，clean
all，clean这一类的是 Makefile 中的伪目标，伪目标并不是一个真正的编译目标，它代表着一系列你想要执行的命令集合，通常一个 Makefile 会对应多个操作，例如编译，清除编译结果，安装，就可以使用这些伪目标来进行标记。在执行时就可以键入：
```
make clean
make install
```
等指令来完成相应的指令操作，当make后不带参数时，默认执行 Makefile 中第一个有效目标的操作。 


关于Makefile的详解可以参考另一系列博客[深入解析Makefile系列](http://www.downeyboy.com/2019/05/14/makefile_series_sumary/)

****  

### make -C /lib/modules/\$(shell uname -r)/build/ M=$(PWD) modules
标准的 make -C 指令是这样的：make -C $KDIR M=$PWD [target]，下面分别介绍每个字段的含义。   

* -C 选项：make -C 表示进入到-C 参数对应的目录中进行编译工作，对应的也是解析目标目录下的Makefile，编译完成时返回。  

* \$KDIR：/lib/modules/$(shell uname -r)/build/，指定内核源码的位置，表示进行内核源码根目录下进行编译。  

    直接在目标板上编译时，内核头文件默认存放在/lib/modules/$(shell uname -r)/build/中，这个build/目录是一个软连接，链接到源码头文件的安装位置。而内核真正的源码库则直接引用正在运行的内核镜像。  

    当跨平台编译时，就需要指定相应的内核源码目录，而不是系统中的源码目录，同时交叉编译时，需要指定架构平台和交叉编译工具链;  

* M=$(PWD)：表示在内核中需要被编译的目录，在示例中也就是当前目录。

* modules，属于[target]部分，事实上，这是个可选选项。默认行为是将源文件编译并生成内核模块，即module(s)，但是它还支持一下选项：

    * modules_install:安装这个外部模块，默认安装地址是/lib/modules/$(uname -r)/extra/，同时可以由内建变量 INSTALL_MOD_PATH 指定安装目录
    * clean:卸载源文件目录下编译过程生成的文件，在上文的 Makefile 最后一行可以看到。
    * help：帮助信息

****  


## 更多选项

### 编译多个源文件
hello_world 总是简单的，但是在实际开发中，就会出现更复杂的情况，这时候就需要了解更多的Makefile选项：

首先，当一个 .o 目标文件的生成依赖多个源文件时，显然 make 的自动推导规则就力不从心了(它只能根据同名推导，比如编译 filename.o，只会去查找 filename.c )，我们可以这样指定：

```  
obj-m  += hello.o
hello-y := a.o b.o hello_world.o
```

hello.o 目标文件依赖于 a.o,b.o,hello_world.o,那么这里的 a.o 和 b.o 如果没有指定源文件，根据推导规则就是依赖源文件 a.c,b.c,hello_world.c.  

除了hello-y，同时也可以用hello-objs,实现效果是一样的。  

***  

### 同时编译多个可加载模块
kbuild支持同时编译多个可加载模块，也就是生成多个.ko文件，它的格式是这样的：

```
obj-m := foo.o bar.o
foo-y := <foo_srcs>
bar-y := <bar_srcs>
```  

就是这么简单。  

***  

### ifneq ($(KERNELRELEASE),)

通常，标准的Makefile会写成这样：

```
ifneq ($(KERNELRELEASE),)
obj-m  := hello.o

else
KDIR ?= /lib/modules/`uname -r`/build

all:
        $(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
        $(MAKE) -C $(KDIR) M=$(PWD) clean
endif
```

为什么要添加一个ifneq，else，all条件判断。  

这得从 linux 内核模块 make 执行的过程说起：当键入 make 时，make 在当前目录下寻找 Makefile 并执行，KERNELRELEASE 在顶层的 Makefile 中被定义，所以在执行当前 Makefile 时KERNELRELEASE 并没有被定义，走 else 分支，直接执行
```
$(MAKE) -C $(KDIR) M=$(PWD) modules
```

而这条指令会进入到 $(KDIR) 目录,也就是源码所在根目录，调用顶层的 Makefile，在顶层 Makefile 中定义了 KERNELRELEASE 变量.  

在顶层 Makefile 中会递归地进入子目录下进行编译，再次调用到当前目录下的 Makefile 文件，这时 KERNELRELEASE 变量已经非空，所以执行if分支，在可加载模块编译列表添加hello模块，由此将模块编译成可加载模块放在当前目录下。   

归根结底，各级子目录中的 Makefile 文件的作用就是先切换到顶层 Makefile ，然后通过 obj-m 在可加载模块编译列表中添加当前模块，kbuild就会将其编译成可加载模块。  

如果是直接在 top Makefile 下编译整个内核源码，就省去了else分支中进入顶层 Makefile 的步骤。  

需要注意的一个基本概念是：每一次编译，顶层 Makefile 都试图递归地进入每个子目录调用子目录的 Makefile，只是当目标子目录中没有任何修改时，默认不再进行重复编译以节省编译时间。  

*** 

## 外部模块头文件的放置
当编译的目标模块依赖多个头文件时，kbuild对头文件的放置有这样的规定：
* 直接放置在 Makefile 同在的目录下，在编译时当前目录会被添加到头文件搜索目录。  
* 放置在系统目录，这个系统目录是源代码目录中的 include/linux/,注意是源代码目录而不是系统目录。  
* 与通用的 Makefile 一样，使用 -I$(DIR) 来指定，不同的是，代表编译选项的变量是固定的，为 ccflag，因为当前 Makefile 相当于是镶嵌到 Kbuild 系统中进行编译，所以必须遵循 Kbuild 中的规定.  

一般的用法是这样的：

```
ccflags-y := -I$(DIR)/include
```

kbuild就会将$(DIR)/include目录添加到编译时的头文件搜索目录中。  

****  
****  
****  


好了，关于linux编译内核模块的Makefile介绍就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.



