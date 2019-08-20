# 内核Kbuild详解系列-10-Kbuild中make_config的实现

在前面我们总共使用了6个章节的篇幅，为最后的 make *config 解析和 vmlinux 的生成做铺垫。   

之后的这三篇博客，就是从 top Makefile 开始，根据源码，讲解我们常用的 make menuconfig 、make 指令执行背后的原理，彻底弄清楚内核镜像到底是怎么被编译出来的。  

对于 make *config 对应的内核配置，通常情况下，我们都是使用 make menuconfig，我们就以 make menuconfig 为例来讲配置的执行过程。  

## top Makefile的处理
当我们执行 make menuconfig 时，对应 top Makefile 中的规则是这样的：
```
%config: scripts_basic outputMakefile FORCE
	$(Q)$(MAKE) $(build)=scripts/kconfig $@
```

目标有两个依赖： scripts_basic 和 outputMakefile，接着我们找到这两个目标的生成规则：   
outputMakefile：
```
outputMakefile:
ifneq ($(srctree),.)
	$(Q)ln -fsn $(srctree) source
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/mkMakefile $(srctree)
	$(Q)test -e .gitignore || \
	{ echo "# this is build directory, ignore it"; echo "*"; } > .gitignore
endif
```

scripts_basic:  
```
scripts_basic:
	$(Q)$(MAKE) $(build)=scripts/basic
	$(Q)rm -f .tmp_quiet_recordmcount
```

看起来整个 make menuconfig 的过程并不复杂，只要弄清楚 %config、outputMakefile、scripts_basic 这三个目标的生成规则即可，其中 outputMakefile、scripts_basic 是 %config 的依赖。  

接下来就是逐个研究这三个目标。  

***  

## outputMakefile
因为 outputMakefile、scripts_basic 是 %config 的依赖，所以先从依赖开始，outputMakefile的定义是这样的：
```
outputMakefile:
ifneq ($(srctree),.)
	$(Q)ln -fsn $(srctree) source
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/mkMakefile $(srctree)
    //版本管理软件的处理
	$(Q)test -e .gitignore || \
	{ echo "# this is build directory, ignore it"; echo "*"; } > .gitignore
endif
```

这部分并不难理解：先判断源码目录(\$(srctree))与当前目录是否相等，如果相等，则不执行任何指令，如果不等，则执行以下两条指令：  

第一：在当前目录下创建源码根目录的软链接

第二：执行 \$(srctree)/scripts/mkMakefile \$(srctree) 这条 shell 指令。
```
cat << EOF > Makefile
include $(realpath $1/Makefile)
EOF
```
很明显，这条语句就是将 \$1/Makefile 也就是 $(srctree)/Makefile 包含(include) 到当前目录的 Makefile 下，效果上相当于将源码根目录下的 Makefile 复制到当前目录下。  

那么这些是什么意思呢？

答案是：当我们没有通过 O=DIR 指定源码与生成文件目录分开存放时，ifneq ($(srctree),.) 这条语句是不成立的，也就是什么都不执行。  

显然，这条指令是为用户指定了 O=DIR 服务的，此时，在指定目录下创建源码的软链接，并且将 top Makefile 复制到该目录的 Makefile 中，然后就跳转到指定目录下执行对应的 Makefile.

***  

## scripts_basic
scripts_basic的定义部分是这样的：
```
scripts_basic:
	$(Q)$(MAKE) $(build)=scripts/basic
	$(Q)rm -f .tmp_quiet_recordmcount
```

由我之前的博客 Makefile.build文件详解 可知,\$(Q)\$(MAKE) \$(build)=scripts/basic 这条指令可以简化成：
```
make -f $(srctree)/scripts/Makefile.build obj=scripts/basic
```

将 \$(srctree)/scripts/Makefile.build 作为 Makefile 执行，并传入参数 obj=scripts/basic。

我们直接查看 scripts/basic/Makefile,因为在 \$(srctree)/scripts/Makefile.build 中包含了它：
```
hostprogs-y	:= fixdep
always		:= $(hostprogs-y)

$(addprefix $(obj)/,$(filter-out fixdep,$(always))): $(obj)/fixdep
```

因为 \$(srctree)/scripts/Makefile 并没有包含目录，整个过程没有出现目录的递归，所以 \$(always) 的值就是 fixdep。   

因为 \$(srctree)/scripts/Makefile 没有指定目标，所以将执行 Makefile.build 中的默认目标：
```
__build: $(if $(KBUILD_BUILTIN),$(builtin-target) $(lib-target) $(extra-y)) \
	 $(if $(KBUILD_MODULES),$(obj-m) $(modorder-target)) \
	 $(subdir-ym) $(always)
	@:
```
因为 \$(KBUILD_BUILTIN) 和 \$(KBUILD_MODULES) 都为空，\$(subdir-ym) 也没有在 Makefile 中设置，所以，在 make menuconfig 时，仅编译 $(always)，也就是 fixdep。  

所以，在生成 scripts_basic 的阶段就干了一件事：生成 fixdep 。

**注：这一部分的理解需要借助前面章节博客中对 Makefile.build 和 fixdep 的讲解**

***  

## %config
两个依赖文件的生成搞定了，那么就来看主要的目标，也就是 menuconfig。  
```
%config: scripts_basic outputMakefile FORCE
	$(Q)$(MAKE) $(build)=scripts/kconfig $@
```

menuconfig也好、oldconfig也好，都将匹配 %config 目标，以 menuconfig 为例，命令部分可以简化为：
```
make -f $(srctree)/scripts/Makefile.build obj=scripts/kconfig menuconfig
```

如果你有看了前面关于 Makefile.build 的解析博客，就知道，对于上述指令，编译流程是：
1. make -f 指令将 Makefile.build 作为目标 Makefile 解析
2. 在 Makefile.build 中解析并包含 scripts/kconfig/Makefile，同时指定编译目标 menuconfig
3. 如果指定了 hostprogs-y ，同时进入 Makefile.host 对 hostprogs-y 部分进行处理


根据 \$(build) 的规则，我们进入到 scripts/kconfig/ 下查看 Makefile 文件，由于指定了编译目标为：menuconfig，我们找到 scripts/kconfig/Makefile 中 menuconfig 部分的定义：
```
ifdef KBUILD_KCONFIG
Kconfig := $(KBUILD_KCONFIG)
else
Kconfig := Kconfig
endif

menuconfig: $(obj)/mconf
	$< $(silent) $(Kconfig)
```

该部分命令可以简化成(忽略 silent 部分，这是打印相关参数,obj=scripts/kconfig/)：

```
menuconfig: $(obj)/mconf
	$(obj)mconf  Kconfig
```

### (obj)/mconf 的生成
依赖 scripts/kconfig/mconf，我们先看依赖文件是怎么生成的：

```
hostprogs-y	+= mconf
// lxdialog 依赖于checklist.c inputbox.c menubox.c textbox.c util.c yesno.c
lxdialog	:= checklist.o inputbox.o menubox.o textbox.o util.o yesno.o

// mconf 依赖于 $(lxdialog) mconf.o
mconf-objs	:= mconf.o $(addprefix lxdialog/, $(lxdialog)) $(common-objs)

HOSTLDLIBS_mconf = $(shell . $(obj)/mconf-cfg && echo $$libs)
$(foreach f, mconf.o $(lxdialog), \
  $(eval HOSTCFLAGS_$f = $$(shell . $(obj)/mconf-cfg && echo $$$$cflags)))

$(obj)/mconf.o: $(obj)/mconf-cfg
$(addprefix $(obj)/lxdialog/, $(lxdialog)): $(obj)/mconf-cfg
```

相对而言，mconf 的生成过程还是比较复杂的：  
![](https://raw.githubusercontent.com/downeyboy/downeyboy.github.io/master/img/post_picture/linux_kernel_makefile_xmind/generate_conf_flow.png)  

根据图中的流程可以看出：mconf 的生成分成3个步骤：
#### mconf.o : 
mconf.o 的生成，依赖于 mconf.c 和 mconf-cfg,mconf.c不用说，mconf-cfg 主要是调用 mconf-cfg.sh 检查主机上的 ncurse 库，整个图形界面就是在 ncurse 图形库上生成的。

#### lxdialog ：
这一部分就是依赖于 scripts/kconfig/ 目录下的各个 .c 编译而成，主要负责图形以及用户操作的支持。  


#### mconf ：hostprogs-y += mconf
对于 mconf 的生成，稍微有点复杂，在这里将mconf 添加到 hostprogs-y 中，但是我们并没有在当前文件中找到 hostprogs-y 的生成规则，答案要从 scripts/Makefile.build 说起。

在 scripts/Makefile.build 中，先是通过 include \$(kbuild-file) 包含了 scripts/kconfig/Makefile，因为在该 Makefile 中指定了 hostprogs-y，所以也将使下面的语句成立：
```
ifneq ($(hostprogs-y)$(hostprogs-m)$(hostlibs-y)$(hostlibs-m)$(hostcxxlibs-y)$(hostcxxlibs-m),)
include scripts/Makefile.host
endif
```
从而包含了 include scripts/Makefile.host，事实上，hostprogs-y 变量中的目标就是在 scripts/Makefile.host 文件中被编译生成的(编译过程并不难，自己去看看吧)。  


### 回到 menuconfig
解决了依赖文件的生成问题，我们再回到 menuconfig 的生成：
```
menuconfig: $(obj)/mconf
	$(obj)mconf  Kconfig
```

生成指令非常简单，就是 mconf  Kconfig，使用 conf 程序来解析 Kconfig，我们继续来看看这两个到底是何方神圣：
* Kconfig ： Kconfig 就是我们经常碰到的内核配置文件，以源码根目录下的配置文件为参数，传递给 conf 程序， 源码根目录下的 Kconfig 大致是这样的：
	```
	mainmenu "Linux/$(ARCH) $(KERNELVERSION) Kernel Configuration"
	comment "Compiler: $(CC_VERSION_TEXT)"
	source "scripts/Kconfig.include"
	source "init/Kconfig"
	source "kernel/Kconfig.freezer"
	source "fs/Kconfig.binfmt"
	source "mm/Kconfig"
	....
	```
	最上级的 Kconfig 文件中全是子目录，不难想到，这又是一颗树，所有子目录下的 Kconfig 文件层层关联，生成一颗 Kconfig 树，为内核配置提供参考
* 了解了上面的知识点，就很容易猜到 conf 命令是干什么的了：它负责层层递归地解析这棵 Kconfig 树，同时，解析完这棵树肯定得留下这棵树相关的信息，这些信息就是以 .config 文件为首的一系列配置文件、头文件。主要包括：include/generated/ 下的头文件 、 include/config/ 下的头文件 .config 等等。


## 其他的配置方式
鉴于本博客是建立在 make menuconfig 的基础上进行解析，对于其他的解析方式，其实也是大同小异，只需要在 scripts/kconfig/Makefile 中找到对应的目标生成规则即可：
```
xconfig: $(obj)/qconf
	$< $(silent) $(Kconfig)

gconfig: $(obj)/gconf
	$< $(silent) $(Kconfig)

menuconfig: $(obj)/mconf
	$< $(silent) $(Kconfig)

config: $(obj)/conf
	$< $(silent) --oldaskconfig $(Kconfig)

nconfig: $(obj)/nconf
	$< $(silent) $(Kconfig)
```  

关于其他的配置方式，原理是类似的，这里就不再啰嗦了。


## 小结
整个 make menuconfig 的流程还是比较简单的，主要是以下步骤：
1. 匹配 top Makefile 中的 %config 目标，其中分两种情况：
	* 指定了 O=DIR 命令参数，需要将所有生成目标转移到指定的 DIR 中
	* 未指定 O=DIR，一切相安无事，按照正常流程编译。
2. 由 %config 的编译指令进入 scripts/kconfig/Makefile 中编译，找到 menuconfig 目标的生成规则
	* 生成 mconf 程序
	* 使用 mconf 读取并解析 Kconfig，mconf主要就是生成图形界面，与用户进行交互，然后把用户的选择记录下来，同时根据各文件夹下 Kbuild 配置 ，生成一系列配置文件，供后续的编译使用。  


****  
**** 

好了，关于 内核Kbuild详解系列-10-Kbuild中make_config的实现 的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.



