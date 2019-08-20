# linux Kbuild详解系列-8-Kbuild中其他通用函数与变量
研究Kbuild系统，了解完它的大致框架以及脚本文件，接下来就非常有必要去了解它的一些通用的变量、规则及函数。    

这样，在一头扎进去时，才不至于在各种逻辑中迷路，这一篇博客，就专门聊一聊整个Kbuid系统中常用的一些"套路";

## top Makefile中的"套路"
说到常用，首当其冲的自然是top Makefile，它是一切的起点，我们来讲讲 top Makefile 中的那些常用的部分。  

### 打印信息中的奥秘
一个软件也好，一份代码也好，如果要研究它，一个最常用的技巧就是尽量获取更多关于它的打印信息。  

幸运的是，top Makefile 提供这么一个参数，我们可以执行下面的命令获取更详细的打印信息：
```
make V=1
``` 
或
```
make V=2
```
V 的全拼为 verbose，表示详细的，即打印更多的信息，在编译时不指定V时，默认 V=0 ，表示不输出编译。  

值得注意的是，V=1 和  V=2 并不是递进的关系，V=1时，它会打印更多更详细的信息，通常是打印出所有执行的指令，当V=2时，它将给出重新编译一个目标的理由。而不是我们自以为的 V=2 比 V=1 打印更多信息。  

同时，我们经常能在 top Makefile 中发现这样的指令：
```
$(Q)$(MAKE) ...
```
这个 \$(Q) 就是根据 V 的值来确定的，当 V=0 时，\$(Q)为空，当 V=1 时，\$(Q)为@。  

在学习Makefile语法时可以知道，在命令部分前添加@，表示执行命令的同时打印该命令到终端。  

**** 


### CONFIG_SHELL
在执行命令时，CONFIG_SHELL 的出场率也非常高，它的原型是这样的：
```
CONFIG_SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	  else if [ -x /bin/bash ]; then echo /bin/bash; \
	  else echo sh; fi ; fi)
```
其实它的作用就是确定当前Kbuild使用机器上的哪个shell解释器。  

在阅读Makefile的时候，为了理解方便，我们可以直接将 CONFIG_SHELL 看成是 /bin/bash。

****  


### FORCE
经常在Makefile中能看到这样的依赖:
```
foo : FORCE
    ...
```

这算是Makefile中的一个使用技巧，FORCE的定义是这样的：
```
FORCE:
```
是的，它是一个目标，没有依赖文件且没有命令部分，由于它没有命令生成FORCE，所以每次都会被更新。  

所以它的作用就是：当FORCE作为依赖时，就导致依赖列表中每次都有FORCE依赖被更新，导致目标每次被重新编译生成。  

***  

### 提高编译效率
在Makefile的规则中，默认是支持隐式规则的推导和内建变量的，通常情况下，使用内建的规则和变量使得Makefile的编写变得非常方便，但是，随之而来缺点就是：第一个降低编译效率，第二个就是在复杂的环境下可能因此引入更多的复杂性。   

降低编译效率是因为在目标的生成规则中，make工具将会在一些情况下尝试使用隐式规则进行解析，扫描文件列表，禁用隐式规则，就免去了这部分操作。  

在top Makefile的开头处，有这么一项定义：
```
MAKEFLAGS += -rR
```
MAKEFLAGS 将在后续的 make 执行中被当做参数传递，而 -r 和 -R 参数不难从 make 的参数列表中找到：
```
-r, --no-builtin-rules      Disable the built-in implicit rules.
-R, --no-builtin-variables  Disable the built-in variable settings.
```
如上所示，-r 取消隐式规则，而 -R 取消内建变量。 

需要注意的是，这个 -rR 参数是定义在 MAKEFLAGS 变量中，而非一个通用设置，只有在使用了 MAKEFLAGS 参数的地方才使能了这个参数配置。  

但是，由于 GNU make 规则的兼容性，在3.x 版本中，make -rR 参数并不会在定义时立即生效，所以需要使用另一种方式来取消隐式规则，下面是一个示例：
```
$(sort $(vmlinux-deps)): $(vmlinux-dirs) ;
```
就像我在 [Makefile详解系列](http://www.downeyboy.com/2019/05/14/Makefile_series_sumary/) 博客中介绍的一样，取消隐式规则的方式就是重新定义规则，上述做法就是这样，用新的规则覆盖隐式规则，而新的规则中并没有定义命令部分，所以看起来就是取消了。  

****  

### 为生成文件建立独立的目录
内核编译的过程中，默认情况下，编译生成的文件与源代码和是混合在一起的，这样看来，难免少了一些美感，程序员们总喜欢层次分明且干净整洁的东西。  

top Makefile 支持 O= 命令参数
```
make O=DIR 
```
表示所有编译生成的中间文件及目标文件放置在以 DIR 为根目录的目录树中，以此保持源代码的纯净。  

这样做是有好处的，生成文件与源代码隔离可以更方便地进行管理，但是会增加一些学习成本。  


****  

### 变量导出
在top Makefile中，经常使用 export 导出某些变量，比如：
```
export abs_srctree abs_objtree
```
这个 export 是 Makefile的关键字，它表示将变量导出到所有由本Makefile调用的子Makefile中，子Makefile可以直接使用这个变量。  

比如，Makefile1 调用 Makefile2，Makefile2 调用 Makefile3，Makefile4。
所以，Makefile2 就可以使用 Makefile1 中导出的变量，Makefile3、Makefile4可以使用Makefile1、Makefile2中导出的变量。
但是Makefile3不能使用Makefile4中导出的，只有在继承关系中使用。  

其实这就是 Makefile 中的一个特性，了解makfile的都知道，只是这个特性在 top Makefile 中使用特别频繁，啰嗦一点也不为过。  
 
***  

## host 软件

### fixdep
除了make本身，Kbuild 系统还需要借助一些编译主机上的软件，才能完成整个内核编译工作，比如 make menuconfig 时，主机需要生成配置界面，又比如生成或者处理一些文件、依赖时，可以直接使用一些软件，这些软件是平台无关的，所以可以直接由编译主机提供。  

fixdep就是这样的一个软件，它的作用解决目标文件的依赖问题，那么到底是什么问题呢？  

当linux编译完成时，我们会发现在很多子目录下都存在 .\*.o.cmd 和 .\*.o.d文件，比如编译完 fdt.c 文件，就会同时生成.fdt.o.cmd 和 .fdt.o.d 文件
(需要注意的是，这个以 . 开头的文件都是隐藏文件)，这两个文件的作用是什么。  

打开一看就知道了，.fdt.o.d文件中包含了 fdt.c 文件的依赖文件，这个文件的由来可以查看 gcc 的选项表，发现有这么一个选项：
```
--MD FILE   write dependency information in FILE (default none)
```
而在编译过程中发现确实是使用了 -MD 这个选项，所以编译过程就生成了对应的 *.d 依赖文件。  

而.fdt.o.cmd 就是调用 fixdep 以 .fdt.o.d 为参数生成的依赖文件，fixdep 专门处理文件编译的依赖问题。  

.fdt.o.cmd 与 .fdt.o.d 不同的地方是 .fdt.o.cmd 多了很多的以 include/config/* 开头的头文件。  

为什么会明明.fdt.o.d就可以描述目标头文件依赖问题，还需要使用fixdep来进一步生成.fdt.o.cmd呢？  

这是因为，我们在使用make menuconfig进行配置的时候，生成.config的同时，生成了 include/generated/autoconf.h 文件，这个文件算是.config的头文件版本(形式上有一些差别，各位朋友可以自己比对一下)，然后在大部分的文件中都将包含这个文件。  

但是问题来了，如果我仅仅是改了.config 中一个配置项，然后重新配置重新生成了 include/generated/autoconf.h 文件，那么依赖 autoconf.h 的所有文件都要重新编译，几乎是相当于重新编译整个内核，这并不合理。  

一番权衡下，想出了一个办法，将 autoconf.h 中的条目全部拆开，并生成一个对应的 include/config/ 目录，目录中包含linux内核中对应的空头文件，当我们修改了配置，比如将 CONFIG_HIS_DRIVER 由 n 改为 y 时，我们就假定依赖于 include/config/his/driver.h 头文件的目标文件才需要重新编译，而不是所有文件。在一定的标准限制下，这种假定是合理的。  

总的来说，我们可以把 fixdep 看做是一个处理文件依赖的主机工具。  


****  
**** 

好了，关于 linux Kbuild详解系列-8-Kbuild中其他通用函数与变量 的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.



