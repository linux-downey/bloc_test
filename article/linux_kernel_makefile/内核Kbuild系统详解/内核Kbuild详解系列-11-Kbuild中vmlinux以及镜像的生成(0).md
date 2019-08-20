# 内核Kbuild详解系列-11-Kbuild中vmlinux以及镜像的生成(0)
在这一章节中，我们将迎来整个 kbuild 系统的重点：vmlinux 的编译生成流程。

## 编译的开始
当我们执行直接键入 make 时，根据 Makefile 的规则，默认执行的目标就是 top Makefile 第一个有效目标，即 all，随后，all的定义如下：
```
all: vmlinux
```
all 依赖于 vmlinux ，它就是整个内核编译的核心所在。    

鉴于整个 vmlinux 编译流程较为复杂，我们先梳理它的框架，以便对其编译过程有一个基本的认识：

同时，查看 vmlinux 的定义：

```
// $(vmlinux-dirs) 依赖于 prepare
$(vmlinux-dirs): prepare
	$(Q)$(MAKE) $(build)=$@ need-builtin=1

// vmlinux-deps 的内容
vmlinux-deps := $(KBUILD_LDS) $(KBUILD_VMLINUX_OBJS) $(KBUILD_VMLINUX_LIBS)

//vmlinux deps 依赖于 $(vmlinux-dirs)
$(sort $(vmlinux-deps)): $(vmlinux-dirs) ;

// vmlinux 目标的命令部分
cmd_link-vmlinux =                                                 \
	$(CONFIG_SHELL) $< $(LD) $(KBUILD_LDFLAGS) $(LDFLAGS_vmlinux) ;    \
	$(if $(ARCH_POSTLINK), $(MAKE) -f $(ARCH_POSTLINK) $@, true)

// vmlinux 的编译规则
vmlinux: scripts/link-vmlinux.sh autoksyms_recursive $(vmlinux-deps) FORCE
	+$(call if_changed,link-vmlinux)
```


根据 vmlinux 的定义，博主将整个 vmlinux 的编译一共分成三部分
* vmlinux-deps -> vmlinux-dirs -> prepare，顾名思义，就是编译工作的准备部分
* vmlinux的依赖：vmlinux-deps 的内容，\$(KBUILD_VMLINUX_OBJS) \$(KBUILD_VMLINUX_LIBS) 分别对应需要编译的内核模块和内核库。
* 把视线放到vmlinux 编译规则的命令部分，所有目标文件编译完成后的链接工作，调用脚本文件对生成的各文件进行链接工作，最后生成 vmlinux。

了解了这三个步骤，就可以开始对每一个步骤进行层层解析了。  

***  

## 准备工作 prepare

准备部分的调用流程为：
```
all -> vmlinux -> vmlinux-deps -> vmlinux-dirs -> prepare
```
我们就从 prepare开始，先来看一下整个 prepare 的框架：  
![](https://raw.githubusercontent.com/downeyboy/downeyboy.github.io/master/img/post_picture/linux_kernel_makefile_xmind/vmlinux-prepare.png)


而 prepare 被调用的地方和定义为：
```
prepare: prepare0 prepare-objtool
```

整个 prepare 没有命令部分，并没有做什么时候的事，转而查看它的两个依赖目标的生成。  

### prepare0
prepare0 的定义如下：
```
prepare0: archprepare
	$(Q)$(MAKE) $(build)=scripts/mod
	$(Q)$(MAKE) $(build)=.
```
查看它的命令部分，发现执行了两个命令部分：
```
$(Q)$(MAKE) $(build)=scripts/mod
```

该指令将进入到 scripts/Makefile.build ,然后包含 scripts/mod/ 下的 Makefile 文件，执行 scripts/mod/Makefile 下的默认目标规则：

```
hostprogs-y	:= modpost mk_elfconfig
always		:= $(hostprogs-y) empty.o

devicetable-offsets-file := devicetable-offsets.h

$(obj)/$(devicetable-offsets-file): $(obj)/devicetable-offsets.s FORCE
	$(call filechk,offsets,__DEVICETABLE_OFFSETS_H__)
```

这部分命令将生成 scripts/mod/devicetable-offsets.h 这个全局偏移文件。  

同时，modpost 和 mk_elfconfig 这两个目标(这是两个主机程序)被赋值给 always 变量，根据 scripts/Makefile.build 中的规则，将在 Makefile.host 中被生成。

modpost 和 mk_elfconfig 是两个主机程序，负责处理模块与编译符号相关的内容。  


我们再来看 prepare0 命令的另一部分：
```
$(Q)$(MAKE) $(build)=.
```
以当前目录也就是源码根目录为 obj 参数执行 \$(build)，\$(build)将在当前目录下搜索 Kbuild 文件或者 Makefile 文件并包含它(Kbuild优先)。   

发现根目录下同时有一个 Kbuild 文件 和 Makefile 文件(也就是top Makefile)，查看 Kbuild 文件的源码发现，该文件中定义了一系列的目标文件，包括：include/generated/bounds.h 、include/generated/timeconst.h、include/generated/asm-offsets.h、missing-syscalls 等等目标文件，用于之后的编译工作。  

**(这里倒是解决了我长时间以来存在的一个疑问：源码根目录下已经存在了一个 top Makefile 了，为什么还会有一个 Kbuild 文件，因为在Kbuild系统中，Kbuild 和 Makefile 都是可以当成 Makfile 进行解析的，且 Kbuild 的优先级比 Makefile 高，原来源码根目录下的 Kbuild是在这里被调用。)**
**同时注意，Kbuild 文件只是在 Kbuild 系统中与 Makefile作用类似，但是，执行make的默认目标依旧是 Makefile 。**


然后，prepare0 依赖了 archprepare 目标，接下来我们继续看 archprepare 部分的实现。

***  

#### archprepare
对于 archprepare，看到 arch 前缀就知道这是架构相关的，这部分的定义与 arch/$(ARCH)/Makefile 有非常大的关系。  
```
archprepare: archheaders archscripts prepare1 scripts
```
archprepare 并没有命令部分，仅仅是依赖四个目标文件：archheaders、archscripts、prepare1、scripts，继续往下跟：

#### archheaders
一番寻找发现，在 top Makefile 中并不能找到 archheaders 的定义部分，事实上，这个目标的定义在 arch/$(ARCH)/Makfile 下定义：
```
archheaders:
	$(Q)$(MAKE) $(build)=arch/arm/tools uapi
```
以 arm 为例，这里再次使用了 \$(build) 命令，表示进入到 scripts/Makefile.build 中，并包含 arch/arm/tools/Makefile,编译目标为 uapi：
```
uapi-hdrs-y := $(uapi)/unistd-common.h
uapi-hdrs-y += $(uapi)/unistd-oabi.h
uapi-hdrs-y += $(uapi)/unistd-eabi.h
uapi:	$(uapi-hdrs-y)
```
uapi 为用户 API 的头文件，包含unistd-common.h、unistd-oabi.h、unistd-eabi.h等通用头文件。  


#### archscripts
事实上，archscripts 表示平台相关的支持脚本，这一部分完全根据平台的需求而定，而且很多平台似乎都没有这一部分的需求，这里就没有详细研究的必要了。  

想要详细了解的朋友，博主提供一个 mips 架构下的 archscripts 的定义：
```
archscripts: scripts_basic
    $(Q)$(MAKE) $(build)=arch/mips/tools elf-entry
    $(Q)$(MAKE) $(build)=arch/mips/boot/tools relocs
```

#### scripts
scripts 表示在编译过程中需要的脚本程序，下面是它的定义:
```
scripts: scripts_basic scripts_dtc
	$(Q)$(MAKE) $(build)=$(@)
```
命令部分同样是 \$(build),并传入 obj=scripts/,同样的，找到 scripts/ 下的 Makefile，Makefile 下的内容是这样的：
```
hostprogs-$(CONFIG_BUILD_BIN2C)  += bin2c
hostprogs-$(CONFIG_KALLSYMS)     += kallsyms
hostprogs-$(CONFIG_LOGO)         += pnmtologo
hostprogs-$(CONFIG_VT)           += conmakehash
hostprogs-$(BUILD_C_RECORDMCOUNT) += recordmcount
hostprogs-$(CONFIG_BUILDTIME_EXTABLE_SORT) += sortextable
hostprogs-$(CONFIG_ASN1)	 += asn1_compiler
...
hostprogs-y += unifdef
build_unifdef: $(obj)/unifdef
	@:
```
整个 scripts/Makefile 编译了一系列的主机程序，包括 bin2c、kallsyms、pnmtologo等，都是需要在编译中与编译后使用到的主机程序。

****  

接着再来看 scripts 依赖的两个目标：scripts_basic 和 scripts_dtc。

scripts_basic 在之前的 make menuconfig 中已经有过介绍，它将生成 fixdep 程序，该程序专门用于解决头文件依赖问题。  

#### scripts_dtc
scripts_dtc 的定义是这样的：
```
scripts_dtc: scripts_basic
	$(Q)$(MAKE) $(build)=scripts/dtc
```
同样的，进入 scripts/dtc/Makefile ，发现文件中有这么一部分：
```
hostprogs-$(CONFIG_DTC) := dtc
always		:= $(hostprogs-y)
dtc-objs	:= dtc.o flattree.o fstree.o data.o livetree.o treesource.o \
		   srcpos.o checks.o util.o
dtc-objs	+= dtc-lexer.lex.o dtc-parser.tab.o
...
```
可以发现，该目标的作用就是生成 dtc (设备树编译器)。

***  

#### prepare1
回到 archprepare 的最后一个目标：prepare1，同样地，查看它的定义：
```
prepare1: prepare3 outputMakefile asm-generic $(version_h) $(autoksyms_h) \
						include/generated/utsrelease.h
	$(cmd_crmodverdir)
```
它执行了 \$(cmd_crmodverdir) 命令，这个命令的原型为：
```
cmd_crmodverdir = $(Q)mkdir -p $(MODVERDIR) \
                  $(if $(KBUILD_MODULES),; rm -f $(MODVERDIR)/*)
```
\$(MODVERDIR) 仅与编译外部模块相关，这里针对外部模块的处理，如果指定编译外部模块，则不做任何事，如果没有指定编译外部模块，清除\$(MODVERDIR)/目录下所有内容。  

\$(MODVERDIR)变量是专门针对于编译外部模块的变量。  

***  

了解了 prepare1 的命令部分，接下来逐个解析它的依赖部分的目标：

#### prepare3
通常，当我们指定了 O=DIR 参数时，发现如果源代码中在之前的编译中有残留的之前编译过的中间文件，编译过程就会报错，系统会提示我们使用 make mrprope 将之前的所有编译中间文件清除，再重新编译。  

这是因为，如果源码根目录下有一份配置文件，而使用 O=DIR 编译，在指定目录 DIR 下也有一份同名文件，将会导致编译过程的二义性，所以 Kbuild 强制性地要求源码的整洁。  

其实，这一部分的检查，就是 prepare3 在做，接着看 prepare3 的定义：
```
prepare3: include/config/kernel.release
ifneq ($(srctree),.)
	@$(kecho) '  Using $(srctree) as source for kernel'
	$(Q)if [ -f $(srctree)/.config -o \
		 -d $(srctree)/include/config -o \
		 -d $(srctree)/arch/$(SRCARCH)/include/generated ]; then \
		echo >&2 "  $(srctree) is not clean, please run 'make mrproper'"; \
		echo >&2 "  in the '$(srctree)' directory.";\
		/bin/false; \
	fi;
endif
```

从源码中可以看出，prepare3 将会检查的文件有 \$(srctree)/include/config、\$(srctree)/arch/$(SRCARCH)/include/generated、\$(srctree)/.config。

这是在源码编译时需要重点注意的一点，如果你的编译报这种错误，你就需要检查这些文件，否则，直接执行 make mrproper 将会导致你的内核源码需要完全地重新编译一次。  



#### outputMakefile
prepare1 的第二个依赖目标，这个目标在 make menuconfig 中有过讲解，它表示在指定 O=DIR 进行编译时，将top Makefile 拷贝到到指定 DIR 下。(不是直接的文件拷贝，但是效果是一致的)。


#### asm-generic、version_h、autoksyms_h、 include/generated/utsrelease.h
这些都是 prepare1 的依赖文件，而且这些依赖都代表一些通用头文件，asm-generic 表示架构相关的头文件。

由于这一部分和内核编译的框架关系不大，细节我们就不再深究。  


## 总结

### prepare小结
到这里，整个 prepare 的部分就分析完毕了，由于其中的调用关系较复杂，在最后，我觉得有必要再对其进行一次整理，整个流程大致是这样的：
* prepare : 没有命令部分。
    * prepare0：生成 scripts/mod/devicetable-offsets.h 文件，并编译生成 modpost 和 mk_elfconfig 主机程序
        * archprepare：没有命令部分
            * archheaders：规则定义在对应的 arch/\$(ARCH)/Makefile 下,负责生成 \$(uapi)/unistd-common.h、\$(uapi)/unistd-oabi.h、\$(uapi)/unistd-eabi.h 等架构体系相关通用头文件
            * archscripts：完全地架构相关的处理脚本，甚至在多数架构中不存在
            * prepare1：在内核编译而不是外部模块编译时，清除\$(MODVERDIR)/目录下所有内容
                * prepare3：当使用 O=DIR 指定生成目标文件目录时，检查源代码根目录是否与指定目标目录中文件相冲突
                * outputMakefile ： 当使用 O=DIR 指定生成目标文件目录时，复制一份 top Makefile 到目标 DIR 目录
                * asm-generic $(version_h) $(autoksyms_h) include/generated/utsrelease.h ： 一系列架构相关的头文件处理。
            * scripts :编译了一系列的主机程序，包括 bin2c、kallsyms、pnmtologo等，都是需要在编译中与编译后使用到的主机程序
                * scripts_basic ： 编译生成主机程序 fixdep
                * scripts_dtc ： 编译生成 dtc
    * prepare-objtool

整个 prepare 部分也就像它的名称一样，不做具体的编译工作，只是默默地做一些检查、生成主机程序、生成编译依赖文件等辅助工作。  


### vmlinux的编译流程回顾
再一次回顾 vmlinux 的编译流程：
* vmlinux-deps -> vmlinux-dirs -> prepare，顾名思义，就是编译工作的准备部分  
* vmlinux的依赖：vmlinux-deps 的内容，\$(KBUILD_VMLINUX_OBJS) \$(KBUILD_VMLINUX_LIBS) 分别对应需要编译的内核模块和内核库。
* 把视线放到vmlinux 编译规则的命令部分，所有目标文件编译完成后的链接工作，调用脚本文件对生成的各文件进行链接工作，最后生成 vmlinux。


下一章节，我们将继续解析编译 vmlinux 的另外两个部分：vmlinux-dep依赖的生成 和 镜像的链接。 


****  
**** 

好了，关于 内核Kbuild详解系列-11-Kbuild中vmlinux以及镜像的生成(0) 的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.


