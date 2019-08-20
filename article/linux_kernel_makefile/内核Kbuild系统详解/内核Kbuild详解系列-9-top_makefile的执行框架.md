# 内核Kbuild详解系列-9-top_Makefile的执行框架

经过前面5个章节一系列的铺垫，我们终于启动了对 top Makefile 的研究。     

由于各内核版本的 Makefile 实际上有一些位置的调整、变量名以及功能的小区别，这里就不逐行地对Makefile进行分析(因为随着版本的变化，Makefile的内容排布肯定会有所变化)，而是以功能为单位，各位可以对照自己研究的Makefile版本找到对应的部分，框架部分是完全通用的。  

## top Makefile 整体框架
当我们执行 make 编译的时候，我们将整个 top Makefile的执行分成以下的逻辑部分：
1. 配置(make *config)：对内核中所有模块进行配置，以确定哪些模块需要编译进内核，哪些编译成外部模块，哪些不进行处理，同时生成一些配置相关的文件供内核真正编译文件时调用。
2. 内核编译(make)：根据配置部分，真正的编译整个内核源码，其中包含递归地进入各级子目录进行编译。 
3. 通过命令行传入 O=DIR 进行编译 ： 所有编译过程生成的中间文件以及目标文件放置在指定的目录，保持源代码的干净。
4. 通过命令行传入多个编译目标，比如：make clean all，make config all，等等，一次指定多个目标
5. 通过命令行传入 M=DIR 进行编译：指定编译某个目录下的模块，并将其编译成外部模块
6. 指定一些完全独立的目标，与内核真实的编译过程并不强相关，比如：make clean(清除编译结果)，make kernelrelease(获取编译后的发型版本号)，make headers_(内核头文件相关)

## Makefile 整体框架解释
为什么要将内核编译过程分为这六个过程呢？  

一方面，这六个过程对 top Makefile 的处理各不相同，且都具有代表性,在这里需要建立的一个概念就是：top Makefile被调用的情况并非仅仅是用户使用 make 或者 make *config指令直接调用，它可能被其它部分调用，所以如果要研究它，就得知道它是怎么被执行的，这些六种情况对应了几种对 top Makefile 不同的调用。  

另一方面，top Makefile的整个结构布局就是针对这几种情况进行处理。  

如果我们不先了解它的框架，其中无数的 ifdef 条件语句只会让我们痛不欲生。根据上一小节中列出的框架，我们对这些部分的编译过程进行详细解析：  
  
* 第1、2 点最常用到，通常就是对应指令 
	```
	make menuconfig
	make
	```
* 第3点，命令行传入 O=DIR 进行编译,它的编译过程是这样的：
	* 先执行 make menuconfig O=DIR，在指定的 DIR 目录下生成 .config 、Makefile(top Makefile的副本)等一系列配置文件
	* 调用 make O=DIR ，进入 top Makefile，在 top Makefile 中进行一些常规变量的处理并 export。
	* 使用 make -C 指令进入到 DIR 中进行编译工作，并指定执行 top Makefile，所以相当于 top Makefile 又被调用了一次。
* 第4点，同时指定多个目标，从用户角度来说，用户并不关心 top Makefile 怎么实现，只知道多个目标将依次被执行，等待结果就可以了。实际上，top Makefile 对多个目标的处理就是对每一个目标都重新调用解析一次 top Makefile，直到所有目标都执行完成
* 第5点，通过命令行传入 M=DIR 进行编译，这种情况下在编译内核可加载模块是最常见的，不知道你还记不记得我们在编译可加载模块时 Makefile 中的这一条语句：
	```
	make -C /lib/modules/\$(shell uname -r)/build/ M=\$(PWD) modules
	```
	make -C /lib/modules/\$(shell uname -r)/build/
	表示进入到指定目录 /lib/modules/\$(shell uname -r)/build/(也就是内核源码目录) 进行编译，指定 M=\$(PWD)，表示进入当前目录进行编译，modules为目标表示编译外部模块。

	所以，在这种情况下，top Makefile事实上是由外部模块中的 Makefile 调用的。

* 第6点： 这一部分与真正的内核源码编译没有太大关系，仅仅是处理一些编译之外的事件，这一部分较为循规蹈矩，没有其他额外的操作。  


## 进入 top Makefile
摸清了整个 top Makefile 大体的脉络，我们就结合上述框架进入到top Makefile 中，一探庐山真面目。  

在整个 top Makefile的研究中，我们将剪去一些细枝末节，将重要的部分挑取出来进行详细讲解，同时并没有严格按照上述的框架顺序进行讲解。  

***  

### 确定打印等级
top Makefile 支持打印等级的设置，参数为命令行传入 V=1|2，默认为0,1表示打印make执行时步骤，2表示打印重新编译的原因。下面是源码部分：
```
ifeq ("$(origin V)", "command line")
  KBUILD_VERBOSE = $(V)
endif
ifndef KBUILD_VERBOSE
  KBUILD_VERBOSE = 0
endif

ifeq ($(KBUILD_VERBOSE),1)
  quiet =
  Q =
else
  quiet=quiet_
  Q = @
endif

ifneq ($(findstring s,$(filter-out --%,$(MAKEFLAGS))),)
  quiet=silent_
endif

export quiet Q KBUILD_VERBOSE
```
这一部分很好理解，主要是以下步骤：
* 判断是否传入 V 参数，如果有，判断是否为 1 。
* 如果传入 V=1 , quiet 变量设置空，Q 设置为@，在后文中经常看到 \$(Q)\$(MAKE) 的指令，如果 Q 为 @，将相当于执行 @\$(MAKE)，在 Makefile 的规则中，命令部分前添加 @ 表示执行的同时打印该命令，以此起到打印执行的效果
* 判断是否传入 -s 选项，如果有，设置 quiet=silent_，表示静默编译。这个 -s 选项等级是最高的，一旦指定这个选项，不仅 V 选项失效，连默认应该打印的log都不输出。
* 将确定的 quiet Q KBUILD_VERBOSE 导出将要到调用到的 Makefile 中。

****  

### 确定是否将生成文件存放到指定目录
这一部分也就是上述所说的，命令行传入 O=DIR 参数，看看源码实现：
```
//第一部分
ifeq ("$(origin O)", "command line")
  KBUILD_OUTPUT := $(O)
endif

ifneq ($(KBUILD_OUTPUT),)

abs_objtree := $(shell mkdir -p $(KBUILD_OUTPUT) && cd $(KBUILD_OUTPUT) && pwd)
$(if $(abs_objtree),, \
     $(error failed to create output directory "$(KBUILD_OUTPUT)"))

# $(realpath ...) resolves symlinks
abs_objtree := $(realpath $(abs_objtree))
else
abs_objtree := $(CURDIR)
endif # ifneq ($(KBUILD_OUTPUT),)

//第二部分
abs_srctree := $(realpath $(dir $(lastword $(Makefile_LIST))))  

//第三部分
ifeq ($(abs_objtree),$(CURDIR))
# Suppress "Entering directory ..." unless we are changing the work directory.
MAKEFLAGS += --no-print-directory
else
need-sub-make := 1
endif

ifneq ($(abs_srctree),$(abs_objtree))
MAKEFLAGS += --include-dir=$(abs_srctree)
need-sub-make := 1
endif

```
其中分为三部分进行解析：
* 第一部分，主要是判断是否指定 O=DIR ，如果指定，赋值给 abs_objtree,并创建abs_objtree目录，获取 abs_objtree 的绝对路径，即指定目录的绝对路径。  
* 第二部分：确定 abs_srctree，即源码根目录的绝对地址。并导出到在本文件中调用的 Makefile 中。  
* 第三部分：判断 abs_objtree 与 \$(CURDIR)是否相等，如果不等，need-sub-make := 1，表示指定目标目录与当前目录不一致，同样的，判断\$(abs_srctree)和\$(abs_objtree)是否相等，如果不等 need-sub-make := 1，表示源码目录和指定目标目录不一致。
	如果是默认编译情况，目标生成目录和源码目录是相等的，当指定了 O=DIR 时，(abs_srctree)和\$(abs_objtree)自然是不等的。need-sub-make 被赋值为1.

need-sub-make 这个变量是控制了什么逻辑呢？  

接着往下看：
```
ifeq ($(need-sub-make),1)

PHONY += $(MAKECMDGOALS) sub-make

$(filter-out _all sub-make $(lastword $(Makefile_LIST)), $(MAKECMDGOALS)) _all: sub-make
	@:

sub-make:
	$(Q)$(MAKE) -C $(abs_objtree) -f $(abs_srctree)/Makefile $(MAKECMDGOALS)

endif # need-sub-make
```
显然，与我们上文中的解析一致，当 need-sub-make 变量值为 1 时，执行: 
```
make -C DIR -f top_Makefile $(MAKECMDGOALS)
```
即跳到指定 DIR 下再次执行 top Makefile。

****  

### 确保命令行变量不被重复解析
在上文的介绍中，博主是按照 top Makefile 中的文本顺序来逐一讲解的，到这里我们了解了 top Makefile 事实上并非仅仅是被用户直接调用，而是可能被重复调用。  

但是在重复调用的情况下，截止 top Makefile 到 上文，都是一些命令行解析，这部分解析是没必要被重复执行的，所以，在 top Makefile开头，我们可以看到这样的一个条件控制语句：
```
ifneq ($(sub_make_done),1)
```
在处理完 ifneq (\$(abs_srctree),\$(abs_objtree)) 这个逻辑分支之后(不管有没有执行sub-make)，修改了该控制参数并结束逻辑控制：
```
export sub_make_done := 1
#endif(correspponding to  sub_make_done)
```
所以在下一次 top Makefile 被调用时，这一部分命令行处理将不再执行。

****  


### M=DIR 编译
当没有指定 O=DIR 参数时，接着往下看 top Makefile 的解析：
```
ifeq ("$(origin M)", "command line")
  KBUILD_EXTMOD := $(M)
endif
```

将 M 参数赋值给变量 KBUILD_EXTMOD，如果没有定义，KBUILD_EXTMOD就为空，我们接着看对 KBUILD_EXTMOD 的处理：

```
ifeq ($(KBUILD_EXTMOD),)
_all: all
else
_all: modules
endif
```
不难看出，如果 KBUILD_EXTMOD 为空，则正常编译默认目标 all，否则，编译目标 modules。  
接着找到 modules 的定义：
```
modules: $(vmlinux-dirs) $(if $(KBUILD_BUILTIN),vmlinux) modules.builtin
	$(Q)$(AWK) '!x[$$0]++' $(vmlinux-dirs:%=$(objtree)/%/modules.order) > $(objtree)/modules.order
	@$(kecho) '  Building modules, stage 2.';
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.modpost
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/modules-check.sh
```
对于真正执行编译的过程，我们在后续的文章中详解，至少到这里，我们确认了命令行中 M=DIR 的用法。  

**** 

### 多个目标的编译
在 Makefile 的规则中，当执行一个 Makefile 时，默认会将目标存在在 MAKECMDGOALS 这个内置变量中，比如执行make clean all，MAKECMDGOALS 的值就是clean all，我们就可以通过查看该变量确定用户输入的编译目标。  

top Makefile 支持各种各样的目标组合，自然也要先扫描用户输入的目标，它的过程是这样的(为理解方便，将部分顺序打乱)：

```
no-dot-config-targets := $(clean-targets) \
			 cscope gtags TAGS tags help% %docs check% coccicheck \
			 $(version_h) headers_% archheaders archscripts \
			 %asm-generic kernelversion %src-pkg

ifneq ($(filter $(no-dot-config-targets), $(MAKECMDGOALS)),)
	ifeq ($(filter-out $(no-dot-config-targets), $(MAKECMDGOALS)),)
		dot-config := 0
	endif
endif
```
其中，no-dot-config-targets表示一些配置目标，但并不是 *config。  

ifneq (\$(filter \$(no-dot-config-targets), \$(MAKECMDGOALS)),)  
和 ifeq (\$(filter-out \$(no-dot-config-targets), \$(MAKECMDGOALS)),) 
这两条语句用于判断编译目标中是否有且仅有 no-dot-config-targets 中的目标，如果是，dot-config 变量设置为 0 。

****  

接着往下看：
```
no-sync-config-targets := $(no-dot-config-targets) install %install \
			   kernelrelease
ifneq ($(filter $(no-sync-config-targets), $(MAKECMDGOALS)),)
	ifeq ($(filter-out $(no-sync-config-targets), $(MAKECMDGOALS)),)
		may-sync-config := 0
	endif
endif
```
no-sync-config-targets 表示那些不需要进行任何编译工作(包括主机上的编译)的目标。  

同样的，上述指令的目的是用于判断目标中是否有且仅有 no-sync-config-targets ，如果是 may-sync-config := 0。

****  

然后：
```
ifneq ($(KBUILD_EXTMOD),)
	may-sync-config := 0
endif

ifeq ($(KBUILD_EXTMOD),)
        ifneq ($(filter config %config,$(MAKECMDGOALS)),)
                config-targets := 1
                ifneq ($(words $(MAKECMDGOALS)),1)
                        mixed-targets := 1
                endif
        endif
endif
```
如果是指定外部模块编译(KBUILD_EXTMOD是用户传入 M=DIR)，may-sync-config := 0  

如果未指定 M=DIR 参数，且用户传入了 *config 目标，config-targets := 1，表示需要处理 *config 目标，比如 make menuconfig。

同时，如果用户传入的目标数量不为 1 ，表示除了 *config 之外还有其他目标，mixed-targets := 1。

****  

```
clean-targets := %clean mrproper cleandocs
ifneq ($(filter $(clean-targets),$(MAKECMDGOALS)),)
        ifneq ($(filter-out $(clean-targets),$(MAKECMDGOALS)),)
                mixed-targets := 1
        endif
endif
```
检查目标中是否有clean-targets，如果有，且指定目标中还有其他 target，则mixed-targets := 1

****   

```
ifneq ($(filter install,$(MAKECMDGOALS)),)
        ifneq ($(filter modules_install,$(MAKECMDGOALS)),)
	        mixed-targets := 1
        endif
endif
```
这一部分是检查用户指定目标中是否有 install，如果有且指定目标中还有其他目标，则 mixed-targets := 1

**** 

目标的解析已经完成，接下来就是相关的处理：
```
ifeq ($(mixed-targets),1)

PHONY += $(MAKECMDGOALS) __build_one_by_one

$(filter-out __build_one_by_one, $(MAKECMDGOALS)): __build_one_by_one
	@:

__build_one_by_one:
	$(Q)set -e; \
	for i in $(MAKECMDGOALS); do \
		$(MAKE) -f $(srctree)/Makefile $$i; \
	done
else
```

如上所示，如果 mixed-targets 的值为 1，表示要执行多个目标，上述的处理就是以每个目标为目标重复调用 top Makefile。

****  
```
ifeq ($(config-targets),1)
	include arch/$(SRCARCH)/Makefile
	export KBUILD_DEFCONFIG KBUILD_KCONFIG CC_VERSION_TEXT

	config: scripts_basic outputMakefile FORCE
		$(Q)$(MAKE) $(build)=scripts/kconfig $@

	%config: scripts_basic outputMakefile FORCE
		$(Q)$(MAKE) $(build)=scripts/kconfig $@
```
如果 config-targets 的值为1，则执行对应的 config指令，关于 make *config 的处理，在后续的博客中详解。  


上述列出的都是一些对于特殊情况的处理，事实上，对于其他普通目标而言，直接找到 top Makefile 中对应的目标生成指令即可，比如：
```
make kernelrelease
``` 
就可以找到对应的
```
kernelrelease:
	@echo "$(KERNELVERSION)$$($(CONFIG_SHELL) $(srctree)/scripts/setlocalversion $(srctree))"

```

****  



### top Makefile其他部分
相信你如果打开了 top Makefile 查看，发现其实里面还包含非常多的内容，相信我，那些东西都是一些纸老虎。  

除了上述提到的内容，top Makefile 当然还有的东西：
* 各个目标的定义实现，编译vmlinux依赖的大量目标，top Makefile 中包含这些目标的编译规则，这一部分将占据 top Makefile 的大量篇幅，事实上我们只需要顺着 vmlinux 的编译规则各个击破就可以了，并没有太多理解上的难点。当然，在下一篇文章我们我们就是要解决这个问题。    
* 各类FLAG，GCC、LD、Makefile_FLAG 等等，这些flag作为执行参数，这一部分我们暂不讨论，不影响我们对 Kbuild 的整体把握
* help 信息，执行make help 返回的帮助信息。


## 小结
在这里，我们再回头看一遍 top Makefile的6大部分：
1. 配置(make *config)：对内核中所有模块进行配置，以确定哪些模块需要编译进内核，哪些编译成外部模块，哪些不进行处理，同时生成一些配置相关的文件供内核真正编译文件时调用。
2. 内核编译(make)：根据配置部分，真正的编译整个内核源码，其中包含递归地进入各级子目录进行编译。 
3. 通过命令行传入 O=DIR 进行编译 ： 所有编译过程生成的中间文件以及目标文件放置在指定的目录，保持源代码的干净。
4. 通过命令行传入多个编译目标，比如：make clean all，make config all，等等，一次指定多个目标
5. 通过命令行传入 M=DIR 进行编译：指定编译某个目录下的模块，并将其编译成外部模块
6. 指定一些完全独立的目标，与内核真实的编译过程并不强相关，比如：make clean(清除编译结果)，make kernelrelease(获取编译后的发型版本号)，make headers_(内核头文件相关)

在这一章节中，我们讲解了第 3、4、5点，关于第6点大家可以直接在 top Makefile 中找到相应的实现，大多并不复杂。  

Kbuild 中的配置和内核源码的真正编译部分留到后续的章节中进行解析。  


****  
**** 

好了，关于 内核Kbuild详解系列-9-top_Makefile的执行框架 的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.







