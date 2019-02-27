# linux内核module makefile
在开发linux内核驱动时，免不了要接触到makefile的编写和修改，尽管网上的makefile模板一大堆，做一些简单的修改就能用到自己的项目上，但是，对于这些基础的东西，更应该做到知其然并知其所以然。  
*** 本篇文章中只讨论linux内核模块编译的makefile，不涉及linux顶层makefile的探究 ***
参考[官方文档](https://github.com/torvalds/linux/blob/master/Documentation/kbuild/modules.txt)了解更多信息。   

linux内核使用的是kbuild编译系统，在编译可加载模块时，其makefile的风格和常用的编译C程序的makefile有所不同，尽管如此，makefile的作用总归是给编译器提供编译信息。

## 最简单的makefile
我们先来看看一个最简单的makefile是怎样的：

        obj-m+=hello.o
        all:
                make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
        clean:
                make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean

这个makefile的作用就是编译hello.c文件，最终生成hello.ko文件。

#### obj-m+=hello.o
obj-m表示编译生成可加载模块。  
相对应的，obj-y表示直接将模块编译进内核。  
可以看到，这里并没有输入hello.c源文件，熟悉makefile的人应该知道，这得益于makefile的自动推导功能，需要编译生成filename.o文件而没有显示地指定filename.c文件位置时，make查找filename.c是否存在，如果存在就正常编译，如果不存在，则报错。  
obj-m+=hello.o，这条语句就是显式地将hello.o编译成hello.ko,而hello.o则由make的自动推导功能编译hello.c文件生成。  

#### all，clean
all，clean这一类的是makefile中的伪目标，伪目标并不是一个真正的编译目标，它代表着一系列你想要执行的命令集合，通常一个makefile会对应多个操作，例如编译，清除编译结果，安装，就可以使用这些伪目标来进行标记。在执行时就可以键入：

        make clean
        make install
等指令来完成相应的指令操作，当make后不带参数时，默认执行第一个伪目标的操作。      
更详细的说明可以参考![官方文档](ftp://ftp.gnu.org/old-gnu/Manuals/make-3.79.1/html_chapter/make_4.html#SEC33) 

#### make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
标准的make指令是这样的：make -C $KDIR M=$PWD [target]，下面分别介绍每个字段的含义。  

-C选项：此选项指定内核源码的位置，make在编译时将会进入内核源码目录，执行编译，编译完成时返回。  
$KDIR：/lib/modules/$(shell uname -r)/build/，指定内核源码的位置。  
直接在目标板上编译时，内核头文件默认存放在/lib/modules/$(shell uname -r)/build/中，而库文件则直接引用正在运行的内核镜像。
当跨平台编译时，就需要指定相应的内核源码目录，而不是系统中的源码目录。交叉编译工具链和架构的问题TODO.
*** 
M=$(PWD)：需要编译的模块源文件地址
*** 
[target]：modules，事实上，这是个可选选项。默认行为是将源文件编译并生成内核模块，即module(s)，但是它还支持一下选项：
* modules_install:安装这个外部模块，默认安装地址是/lib/modules/$(uname -r)/extra/，同时可以由内建变量INSTALL_MOD_PATH指定安装目录
* clean:卸载源文件目录下编译过程生成的文件，在上文的makefile最后一行可以看到。
* help：帮助信息

## 更多选项
### 编译多个源文件
hello_world总是简单的，但是在实际开发中，就会出现更复杂的情况，这时候就需要了解更多的makefile选项：

首先，当一个.o目标文件的生成依赖多个源文件时，显然make的自动推导规则就力不从心了(它只能根据同名推导，比如编译filename.o，只会去查找filename.c)，我们可以这样指定：

        obj-m  += hello.o
        hello-y := a.o b.o hello_world.o
hello.o目标文件依赖于a.o,b.o,hello_world.o,那么这里的a.o和b.o如果没有指定源文件，根据推导规则就是依赖源文件a.c,b.c,hello_world.c.  
除了hello-y，同时也可以用hello-objs,实现效果是一样的。  

### ifneq ($(KERNELRELEASE),)
通常，标准的makefile会写成这样：

        ifneq ($(KERNELRELEASE),)
        obj-m  := hello.o

        else
        KDIR ?= /lib/modules/`uname -r`/build

        all:
                $(MAKE) -C $(KDIR) M=$(PWD) modules
        clean:
                $(MAKE) -C $(KDIR) M=$(PWD) clean
        endif
为什么要添加一个ifneq，else，all条件判断。  
这得从linux内核模块make执行的过程说起：当键入make时，make在当前目录下寻找makefile并执行，KERNELRELEASE在顶层的makefile中被定义，所以在执行当前makefile时KERNELRELEASE并没有被定义，走else分支，






