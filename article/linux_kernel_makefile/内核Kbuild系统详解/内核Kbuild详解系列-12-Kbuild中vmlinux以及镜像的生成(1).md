# linux Kbuild 详解系列(12)-Kbuild中vmlinux以及镜像的生成(1)
在上一章节中，我们讲到了编译生成 vmlinux 的框架以及 prepare 部分，从这一章节开始，我们继续解析后续的两部分。


先回顾一下，在上一章节中我们将vmlinux的编译过程分解的三部分：
* vmlinux-deps -> vmlinux-dirs -> prepare，顾名思义，就是编译工作的准备部分
* vmlinux-deps 的内容，\$(KBUILD_VMLINUX_OBJS) \$(KBUILD_VMLINUX_LIBS) 分别对应需要编译的内核模块和内核库。
* 看到 vmlinux 的命令部分，模块编译完成的链接工作，调用脚本文件对生成的各文件进行链接工作，最后生成 vmlinux。

## 源码的编译
接下来我们要看的就是第二部分，源码部分的编译。这部分对应的变量为 vmlinux-deps 以及 vmlinux-dirs。   

### vmlinux-deps

vmlinux-deps 的被调用流程为： all -> vmlinux -> vmlinux-deps ,而 vmlinux-deps 的值为：

```
// 各变量的具体值
init-y		:= init/
drivers-y	:= drivers/ sound/
drivers-$(CONFIG_SAMPLES) += samples/
net-y		:= net/
libs-y		:= lib/
core-y		:= usr/
virt-y		:= virt/
// 每个变量的值原本是目录，将目录名后加上 built-in.a 后缀
init-y		:= $(patsubst %/, %/built-in.a, $(init-y))
core-y		:= $(patsubst %/, %/built-in.a, $(core-y))
drivers-y	:= $(patsubst %/, %/built-in.a, $(drivers-y))
net-y		:= $(patsubst %/, %/built-in.a, $(net-y))
libs-y1		:= $(patsubst %/, %/lib.a, $(libs-y))
libs-y2		:= $(patsubst %/, %/built-in.a, $(filter-out %.a, $(libs-y)))
virt-y		:= $(patsubst %/, %/built-in.a, $(virt-y))

// vmlinux 相关的目标文件
KBUILD_VMLINUX_OBJS := $(head-y) $(init-y) $(core-y) $(libs-y2) \
			      $(drivers-y) $(net-y) $(virt-y)
// vmlinux 的链接脚本
KBUILD_LDS          := arch/$(SRCARCH)/kernel/vmlinux.lds
// vmlinux 相关的库
KBUILD_VMLINUX_LIBS := $(libs-y1)

vmlinux-deps := $(KBUILD_LDS) $(KBUILD_VMLINUX_OBJS) $(KBUILD_VMLINUX_LIBS)
```
从 vmlinux-deps 的名称可以看出，这个变量属于 vmlinux 的依赖，同时从定义可以看出，它的主要部分就是源文件根目录下的各子目录中的 built-in.a。  

在之前 Makefile.build 的章节中我们有提到编译的总体规则：通常，源码目录如 drivers/ net/ 等目录下的 Makefile 只需要提供被编译的文件和需要递归进行编译的目录，每个对应的目录下都将所有编译完成的目标文件打包成一个 built-in.a 的库文件，然后上级的 built-in.a 库文件同时会将子目录中的 built-in.a 文件打包进自己的built-in.a,层层包含。  

最后，所有的 built-in.a 文件都被集中在根目录的一级子目录中，也就是上文中提到的 init/built-in.a，lib/built-in.a、drivers/built-in.a 等等，vmlinux-deps 变量的值就是这些文件列表。  

那么，这个递归的过程是怎么开始的呢？也就是说，top makefile 是从哪里开始进入到子目录中对其进行递归编译的呢？   

我们还得来看另一个依赖 vmlinux-dirs。

***  

### vmlinux-dirs
vmlinux-dirs 是 vmlinux-deps 的依赖目标，同样的，我们可以轻松地找到vmlinux-dirs的定义部分：
```
// init-y 等值的定义
init-y		:= init/
drivers-y	:= drivers/ sound/
drivers-$(CONFIG_SAMPLES) += samples/
net-y		:= net/
libs-y		:= lib/
core-y		:= usr/
virt-y		:= virt/

// vmlinux-dirs 的赋值
vmlinux-dirs	:= $(patsubst %/,%,$(filter %/, $(init-y) $(init-m) \
		     $(core-y) $(core-m) $(drivers-y) $(drivers-m) \
		     $(net-y) $(net-m) $(libs-y) $(libs-m) $(virt-y)))

// vmlinux-dirs 的编译部分
$(vmlinux-dirs): prepare
	$(Q)$(MAKE) $(build)=$@ need-builtin=1
```

第一部分 vmlinux-dirs 的赋值，就是将原本 \$(init-y) \$(init-m) \$(core-y) \$(core-m)中的 / 去掉，也就是说只剩下目录名，接下来对于其中的每一个元素(init、drivers、net、sound等等)，都调用：
```
$(Q)$(MAKE) $(build)=$@ need-builtin=1
```
我相信你肯定还记得 \$(build) 的作用，就是将 scripts/Makefile.build 作为目标 makefile 执行，并包含指定目录下的 Makefile ，然后执行第一个有效目标。  

如果你看到这里还不懂，鉴于这一部分是各级目录下文件编译的核心部分，博主就再啰嗦一点：
```
$(vmlinux-dirs): prepare
	$(Q)$(MAKE) $(build)=$@ need-builtin=1
```
由于 \$(vmlinux-dirs) 的值为init、drivers、 sound、等等源码根目录下一级子目录名，根据 makefile 规则，相当于执行以下一系列的命令：
```
init: prepare
    $(Q)$(MAKE) $(build)=init need-builtin=1
drivers: prepare
    $(Q)$(MAKE) $(build)=drivers need-builtin=1
sound: prepare
    $(Q)$(MAKE) $(build)=sound need-builtin=1
... 
```
所以，makefile 的执行流程跳到 Makefile.build 中，并包含 init ，取出 init/Makefile 中 obj-y/m 等变量，经过 Makefile.lib 处理，然后执行Makefile.build 的默认目标 __build 进行编译工作，编译完本makefile中的目标文件时，再递归地进入子目录进行编译。   

最后递归返回的时候，将所有的目标文件打包成 built-in.a 传递到 init/built-in.a 中，同样的道理适用于 drivers sound等一干目录,lib/ 目录稍有不同，它同时对应 lib.a 和 built-in.a 。  

至此，vmlinux 的依赖文件 vmlinux-deps ，也就是各级子目录下的 built-in.a 库文件就已经生成完毕了。  

***  

## 链接与符号处理
当所有的目标文件已经就绪，那么就是 vmlinux 生成三部曲的最后一部分：将目标文件链接成 vmlinux。  

回到 vmlinux 编译的规则定义：
```
cmd_link-vmlinux =                                                 \
	$(CONFIG_SHELL) $< $(LD) $(KBUILD_LDFLAGS) $(LDFLAGS_vmlinux) ;    \
	$(if $(ARCH_POSTLINK), $(MAKE) -f $(ARCH_POSTLINK) $@, true)

vmlinux: scripts/link-vmlinux.sh autoksyms_recursive $(vmlinux-deps) FORCE
	+$(call if_changed,link-vmlinux)
```  

在此之前，vmlinux 的依赖文件处理情况是这样的：
* scripts/link-vmlinux.sh,这是源码自带的用于链接的脚本文件。
* $(vmlinux-deps) 根目录的一级子目录下的各个 built-in.a 文件。
* autoksyms_recursive：内核的符号优化


### autoksyms_recursive
对于 autoksyms_recursive ，它也有对应的生成规则:
```
autoksyms_recursive: $(vmlinux-deps)
ifdef CONFIG_TRIM_UNUSED_KSYMS
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/adjust_autoksyms.sh \
	  "$(MAKE) -f $(srctree)/Makefile vmlinux"
endif
```

如果在源码的配置阶段定义了 CONFIG_TRIM_UNUSED_KSYMS 这个变量，就执行：
```
$(Q)$(CONFIG_SHELL) $(srctree)/scripts/adjust_autoksyms.sh \
	  "$(MAKE) -f $(srctree)/Makefile vmlinux"
```

首先，CONFIG_TRIM_UNUSED_KSYMS 默认配置下是没有被定义的，所以的默认情况下，这里的脚本并不会执行。  

其次，这个命令做了什么呢？  

从名称就可以看出，它的作用是裁剪掉内核中定义但未被使用的内核符号，以此达到精简内核文件大小的目的。  

当这个变量被设置和不被设置时，可以发现，System.map 文件的大小有比较大的变化，默认情况下不设置，System.map 的文件内容比设置该变量大了1/4(5.2版本内核)，但是这样的坏处自然是内核提供了更少的服务，尽管这些服务很可能用不上。  


### 链接部分
所有的依赖文件准备就绪，就来看 vmlinux 的命令部分：
```
cmd_link-vmlinux =                                                 \
	$(CONFIG_SHELL) $< $(LD) $(KBUILD_LDFLAGS) $(LDFLAGS_vmlinux) ;    \
	$(if $(ARCH_POSTLINK), $(MAKE) -f $(ARCH_POSTLINK) $@, true)

vmlinux: scripts/link-vmlinux.sh autoksyms_recursive $(vmlinux-deps) FORCE
	+$(call if_changed,link-vmlinux)
```

根据 if_changed 的函数定义，该函数将转而执行 cmd_\$1,也就是 cmd_link-vmlinux。  

从上文中可以看到，对于架构 ARCH_POSTLINK 需要做特殊处理，其他的就是直接执行 scripts/link-vmlinux.sh 脚本程序。  


scripts/link-vmlinux.sh 的执行过程是这样的：
* 链接 \$(KBUILD_VMLINUX_OBJS)，即根目录一级子目录下的 built-in.a
* 链接 \$(KBUILD_VMLINUX_LIBS)，即 lib/lib.a 等。
* 目标文件中的所有符号处理，提取出所有目标文件中的符号，并记录下内核符号 __kallsyms ，基于平台计算出符号在系统中运行的地址，并生成相应的记录文件，包括：System.map、 include/generate/autoksyms.h 等等供系统使用，到系统启动之后 modprobe 还需要根据这些符号对模块进行处理。    



### 生成镜像
vmlinux 可以作为内核镜像使用，但是，通常在嵌入式系统中，基于内存的考虑，系统并不会直接使用 vmlinux 作为镜像，而是对 vmlinux 进行进一步的处理，最普遍的是两步处理：

去除 vmlinux 中的符号，将符号文件独立出来，减小镜像所占空间，然后，对镜像进行压缩，同时在镜像前添加自解压代码，这样可以进一步地减小镜像的大小。  

别小看省下的这些空间，在嵌入式硬件上，空间就代表着成本。  

最后生成的镜像通常被命名为：vmlinuz、Image、zImage、bzImage(这些文件名的区别在前文中可参考) 等等。  

对于其中的压缩部分，我们暂时不去理会，我们可以看看裁剪 vmlinux 中的符号是怎么完成的，当然，这些部分都是平台相关的，所以被放置在 arch/\$(ARCH)/Makefile 目录下进行处理，定义部分是这样的( 以arm平台为例 )：
```
//boot 对应目录
boot := arch/arm/boot
//实际镜像
BOOT_TARGETS	= zImage Image xipImage bootpImage uImage
//镜像生成规则
$(BOOT_TARGETS): vmlinux
	$(Q)$(MAKE) $(build)=$(boot) MACHINE=$(MACHINE) $(boot)/$@
```

根据 \$(build) 的实现，进入到 arch/arm/boot/Makefile 中，以 Image 为例，Image 对于 vmlinux 的操作仅是去除符号，没有压缩操作：
```
//Image 生成的规则
$(obj)/Image: vmlinux FORCE
	$(call if_changed,objcopy)
```
但是在当前 makefile 中找不到对应的 cmd_objcopy，在一番搜索后，发现其在 scripts/Makefile.lib 中被定义, 看来虽然这个命令即使是在 arch/$(ARCH)/ 下调用，也是一个通用命令，下面是它的定义部分：

```
//在arch/$(ARCH)/boot/Makefile 下定义
OBJCOPYFLAGS	:=-O binary -R .comment -S
//scripts/Makefile.lib
cmd_objcopy = $(OBJCOPY) $(OBJCOPYFLAGS) $(OBJCOPYFLAGS_$(@F)) $< $@
```

objcopy 是linux下一个对二进制文件进行操作的命令，主要功能就是将文件中指定的部分复制到目标文件中。  

结合上面 Image 的生成规则，发现这一部分的工作主要就是使用 objcopy 将 vmlinux 中的内容复制到 Image 中，其中我们需要关注它的 flag 部分：
```
-O binary -R .comment -S
```
查询手册可以看到该 flag 对应的功能：
* -O binary：表示输出格式为 binary，也就是二进制
* -R .comment：表示移除二进制文件中的 .comment 段，这个段主要用于 debug
* -S ： 表示移除所有的符号以及重定位信息

以上就是由 vmlinux 生成 Image 的过程。  

同样的，zImage 是怎么生成的呢？ 从 zImage 的前缀 z 可以了解到，这是经过 zip 压缩的 Image，它的规则是这样的：
```
$(obj)/zImage:	$(obj)/compressed/vmlinux FORCE
	$(call if_changed,objcopy)
``` 
与 Image 不同的是，zImage 依赖于 \$(obj)/compressed/vmlinux ，而不是原生的 vmlinux，可以看出是经过压缩以及添加解压缩代码的 vmlinux，具体细节就不再深入了，有兴趣的朋友可以深入研究。  


## 小结
到这里，整个linux内核镜像的编译就结束了，你可以在 arch/\$(ARCH)/boot 目录下找到你想要的启动文件。  

总结一下，镜像编译过程三个步骤：
* 准备阶段，对应的规则为 prepare，主要在 top makefile 和 arch/\$(ARCH)/Makefile 中实现，其中 arch* 部分的目标生成都依赖于 arch/\$(ARCH)/Makefile。
* 递归编译阶段，这一阶段将递归地编译内核源码，并在每个根目录的一级子目录下生成 built-in.a 库。 
* 链接阶段，链接所有的 built-in.a 库到 vmlinux.o，并处理所有相关的符号，生成vmlinux，最后为生成镜像，对 vmlinux 进行符号精简以及压缩操作。  



****  
**** 

好了，关于 内核Kbuild详解系列-13-Kbuild中vmlinux以及镜像的生成(1) 的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.


