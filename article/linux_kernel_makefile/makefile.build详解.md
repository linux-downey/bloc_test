# makefile.build 文件解析
在Kbuild系统中，makefile.build文件算是最重要的文件之一了，它控制着整个内核的核心编译部分，接下来我们来揭开Makefile.build的面纱。   

## makefile.build入口
既然使用到Makefile.build,那不免产生一个疑问，它是怎么被用到的呢？  

答案是：在Top Makefile中，通常可以看到这样的编译结构：

```
scripts_basic:
	$(Q)$(MAKE) $(build)=scripts/basic
    $(Q)rm -f .tmp_quiet_recordmcount
```

如果你对Makfile的基本规则有一个基本的了解，就知道这是一条编译规则，分别对应：
```
目标 : 依赖
       编译命令
```
 

我们重点关注命令部分：
```
$(Q)$(MAKE) $(build)=scripts/basic
```

研究一番发现：非常奇怪的是在整个Makefile中，我们都找不到\$(build)的定义，那它是到底有什么作用呢？  

答案是： build 变量被定义在Kbuild.include中。


在Top Makefile中，有这么一行：
```
include scripts/Kbuild.include
```
在top makefile中包含了scripts/Kbuild.include文件，紧接着在该文件下找到build的相应的定义：
```
build := -f $(srctree)/scripts/Makefile.build obj
```

所以，上述的命令部分 \$(Q)\$(MAKE) \$(build)=scripts/basic 展开过程就是这样：
```
$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.build obj=scripts/basic
```

一目了然，它的作用就是进入到 Makefile.build 中进行编译，并将obj赋值为scripts/basic。  

****  

## makefile.build的执行
在上一章节中提到了，makefile.build的调用基本上都是由 $(build)的使用而开始的，这一部分就来看看makefile.build具体做了什么。  

### 初始化变量
```
// 编译进内核的文件(夹)列表
obj-y :=

// 编译成外部可加载模块的列表
obj-m :=

// 编译成库文件
lib-y :=
lib-m :=

// 总是需要被编译的模块
always :=

//编译目标
targets :=

// 下两项表示需要递归进入的子目录
subdir-y :=
subdir-m :=

//下列表示编译选项
EXTRA_AFLAGS   :=
EXTRA_CFLAGS   :=
EXTRA_CPPFLAGS :=
EXTRA_LDFLAGS  :=
asflags-y  :=
ccflags-y  :=
cppflags-y :=
ldflags-y  :=

//子目录中的编译选项
subdir-asflags-y :=
subdir-ccflags-y :=
```  
上述变量就是当前内核编译需要处理的变量，通常最主要的就是 obj-y 和 obj-m 这两项，分别代表需要被编译进内核的模块和外部可加载模块。  

lib-y 并不常见，通常地，它只会重新在 lib/目录下，其他部分我们在后文继续解析。  

****   

### 包含文件
初始化完成基本变量之后，我们接着往下看：
```
1 src := $(obj)
2 
3 //尝试包含 include/config/auto.conf 
4 -include include/config/auto.conf
5 
6 //该文件中包含大量通用函数和变量的实现
7 include scripts/Kbuild.include
8
9 //如果obj是以/目录开始的目录，kbuild-dir=boj，否则kbuild-dir = srctree/obj
10 kbuild-dir := $(if $(filter /%,$(src)),$(src),$(srctree)/$(src))

11 //如果kbuild-dir存在Kbuild，则kbuild-file为Kbuild，否则为Makefile
12 kbuild-file := $(if $(wildcard $(kbuild-dir)/Kbuild),$(kbuild-dir)/Kbuild,$(kbuild-dir)/Makefile)
13
14 include $(kbuild-file)
15
16 include scripts/Makefile.lib

```
为清晰起见，博主直接在贴出的源码中做了相应注释，下面是他们对应的详细解读：
1. 将 obj 变量赋值给 src。那么，obj的值是哪儿来的呢？一般情况下，\$(build)的使用方式为：\$Q\$MAKE \$(build)=scripts/basic,\$(build)展开之后 obj 就被赋值为 scripts/basic。
2. 尝试包含include/config/auto.conf文件，这个文件的内容是这样的：
    ```
    CONFIG_RING_BUFFER=y
    CONFIG_HAVE_ARCH_SECCOMP_FILTER=y
    CONFIG_SND_PROC_FS=y
    CONFIG_SCSI_DMA=y
    CONFIG_TCP_MD5SIG=y
    CONFIG_KERNEL_GZIP=y
    ...
    ```
    可以看出，这是对于所有内核模块的配置信息，include前添加"-"表示它可能include失败，makefile对于错误的默认处理就是终止并退出，"-"表示该语句失败时不退出并继续执行，因为include/config/auto.conf这个文件并非内核源码自带，而是在运行make menuconfig之后产生的。  
3. 包含scripts/Kbuild.include，相当于导入一些通用的处理函数 
4. 9-14行，处理通过\$(build)=\$(build)=scripts/basic传入的scripts/basic目录，获取该目录的相对位置，如果该目录下存在Kbuild文件，则包含该Kbuild(在Kbuild系统中，make优先处理Kbuild而不是makefile)文件，否则查看是否存在Makefile文件，如果存在Makefile，就包含Makefile文件，如果两者都没有，就是一条空包含指令。  
    这一部分指令的意义在于：\$(build)=scripts/basic 相当于 make -f \$(srctree)/scripts/Makefile.build obj=scripts/basic,如果obj=scripts/basic下存在makefile或者Kbuild文件，就默认以该文件的第一个有效目标作为编译目标，否则，就默认以$(srctree)/scripts/Makefile.build下的第一个有效目标作为编译目标，即__build目标。(make工具的编译规则，以目标处理文件中第一个有效目标作为编译目标)  
    同样的，遵循make的规则，用户也可以指定编译目标，比如：
    ```
    scripts_basic:
	    $(Q)$(MAKE) $(build)=scripts/basic $@
    ```
    这样，该规则展开就变成了：
    ```
    scripts_basic:
	    $(Q)$(MAKE) -f $(srctree)/scripts/Makefile.build obj=scripts/basic scripts_basic
    ```
    表示指定编译scripts/basic/makefile 或 scripts/basic/Kbuild文件下的scripts_basic目标(这里仅为讲解方便，实际上scripts/basic/makefile中并没有scripts_basic目标)。
5. 包含include scripts/Makefile.lib，lib-y，lib-m等决定编译文件对象的变量就是在该文件中进行处理。


### 编译目标文件的处理
接下来就是代表编译目标文件的处理部分:
```
//主机程序，在前期的准备过程中可能需要用到，比如make menuconfig时需要准备命令行的图形配置。
1 ifneq ($(hostprogs-y)$(hostprogs-m)$(hostlibs-y)$(hostlibs-m)$(hostcxxlibs-y)$(hostcxxlibs-m),)
2 include scripts/Makefile.host
3 endif

//判断obj，如果obj没有指定则给出警告
4 
5 ifndef obj
6 $(warning kbuild: Makefile.build is included improperly)
7 endif

//如果有编译库的需求，则给lib-target赋值，并将 $(obj)/lib-ksyms.o 追加到 real-obj-y 中。
8 
9 ifneq ($(strip $(lib-y) $(lib-m) $(lib-)),)
10 lib-target := $(obj)/lib.a
11 real-obj-y += $(obj)/lib-ksyms.o
12 endif

//如果需要编译 将要编译进内核(也就是obj-y指定的文件) 的模块，则赋值 builtin-target
13 
14 ifneq ($(strip $(real-obj-y) $(need-builtin)),)
15 builtin-target := $(obj)/built-in.a
16 endif

// 如果定义了 CONFIG_MODULES，则赋值 modorder-target。
17 ifdef CONFIG_MODULES
18 modorder-target := $(obj)/modules.order
19 endif
```
同样的，对于每一部分博主都添加了一部分注释，需要特别解释的有两点：
1. 13-16行部分，事实上，如果我们进入到每个子目录下查看Makefile文件，就会发现：obj-m 和 obj-y的赋值对象并不一定是xxx.o这种目标文件， 也可能是目录，real-obj-y可以理解为解析完目录的真正需要编译的目标文件。
    need-builtin变量则是在需要编译内核模块时被赋值为1，  
    根据15行的 builtin-target := $(obj)/built-in.a可以看出，对于目录(值得注意的是，并非所有目录)的编译，都是先将该目录以及子目录下的所有编译进内核的目标文件打包成 built-in.a，在父目录下将该build-in.a打包进父目录的build-in.a，一层一层地往上传递。  
2. 17-19行部分，CONFIG_MODULES是在.config中被定义的，这个变量被定义的条件是在make menuconfig时使能了Enable loadable module support选项，这个选项表示内核是否支持外部模块的加载，一般情况下，这个选项都会被使能。  
    所以，modorder-target将被赋值为$(obj)/modules.order，modules.order文件内容如下：
    ```
    kernel/fs/efivarfs/efivarfs.ko
    kernel/drivers/thermal/intel/x86_pkg_temp_thermal.ko
    kernel/net/netfilter/nf_log_common.ko
    kernel/net/netfilter/xt_mark.ko
    kernel/net/netfilter/xt_nat.ko
    kernel/net/netfilter/xt_LOG.ko
    ....
    ```
    module.order这个文件记录了可加载模块在Makefile中出现的顺序，主要是提供给modprobe程序在匹配时使用。  
    
    与之对应的还有module.builtin文件和modules.builtin.modinfo。  

    顾名思义，module.builtin记录了被编译进内核的模块。

    而modules.builtin.modinfo则记录了所有被编译进内核的模块信息，作用跟modinfo命令相仿，每个信息部分都以模块名开头，毕竟所有模块写在一起是需要做区分的。  

    这三个文件都是提供给modprobe命令使用，modporbe根据这些信息，完成程序的加载卸载以及其他操作。  


### 默认编译目标 
在上文中有提到，当命令为\$(build) = $(xxx_dir),且xxx_dir下没有对应的makefile或Kbuild文件时或者该文件下不存在有效目标时，就编译makefile.build下的默认目标，即 __build 目标，事实上，这种情况是比较多的，其定义如下：
```
__build: $(if $(KBUILD_BUILTIN),$(builtin-target) $(lib-target) $(extra-y)) \
	 $(if $(KBUILD_MODULES),$(obj-m) $(modorder-target)) \
	 $(subdir-ym) $(always)
	@:
```
可以看到，这个默认目标没有对应的命令，只有一大堆依赖目标。而根据make的规则，当依赖目标不存在时，又会递归地生成依赖目标。

以以往的经验来看，对于这些依赖文件，基本上都是通过一些规则生成的，我们可以继续在当前文件下寻找它们的生成过程。  


接下来我们逐个将其拆开来看：

#### builtin-target 
builtin-target ： 在上文中可以看到，这个变量的值为：\$(obj)/built-in.a，也就是当前目录下的built-in.a文件，这个文件在源文件中自然是没有的，需要编译生成，我们在后文中寻找它的生成规则：


可以找到这样一条：
```
$(builtin-target): $(real-obj-y) FORCE
	$(call if_changed,ar_builtin)
```

在前面的博客中有提到，\$(call if_changed,ar_builtin) 这条指令结果将是执行。 cmd_ar_builtin,然后在makefile.build中找到对应的命令：
```
real-prereqs = $(filter-out $(PHONY), $^)
cmd_ar_builtin = rm -f $@; $(AR) rcSTP$(KBUILD_ARFLAGS) $@ $(real-prereqs)
```
不难从源码看出，cmd_ar_builtin 命令的作用就是将所有当前模块下的编译目标(即依赖文件)全部使用 ar 指令打包成 $(builtin-target),也就是 \$(obj)/built-in.a，它的依赖文件被保存在 \$(real-obj-y) 中。 

****  

#### lib-target
lib-target : 同样，这个变量是在本文件中定义，它的值为：$(obj)/lib.a。  

通常，只有在/lib 目录下编译静态库时才会存在这个目标，同时，我们可以在本文件中找到它的定义：
```
$(lib-target): $(lib-y) FORCE
	$(call if_changed,ar)
```  
同样的，\$(call if_changed,ar) 最终将调用 cmd_ar 命令，找到 cmd_ar 指令的定义是这样的：
```
real-prereqs = $(filter-out $(PHONY), $^)
cmd_ar = rm -f $@; $(AR) rcsTP$(KBUILD_ARFLAGS) $@ $(real-prereqs)
```
看来这个命令的作用也是将本模块中的目标全部打包成\$(lib-target),也就是 $(obj)/lib.a，只不过它的依赖是 \$(lib-y) 。


#### $(extra-y) 
\$(extra-y) 在 makefile.lib 中被确定，主要负责 dtb 相关的编译，定义部分是这样的：
```
extra-y				+= $(dtb-y)
extra-$(CONFIG_OF_ALL_DTBS)	+= $(dtb-)
```
在 makefile.lib 中可以找到对应的规则实现：
```
$(obj)/%.dtb: $(src)/%.dts $(DTC) FORCE
	$(call if_changed_dep,dtc,dtb)
```
该命令调用了 cmd_dtc :
```
cmd_dtc = mkdir -p $(dir ${dtc-tmp}) ; \
	$(HOSTCC) -E $(dtc_cpp_flags) -x assembler-with-cpp -o $(dtc-tmp) $< ; \
    
	$(DTC) -O $(2) -o $@ -b 0 \
		$(addprefix -i,$(dir $<) $(DTC_INCLUDE)) $(DTC_FLAGS) \
		-d $(depfile).dtc.tmp $(dtc-tmp) ; \

	cat $(depfile).pre.tmp $(depfile).dtc.tmp > $(depfile)
```
其中，以 $(DTC) 开头的第二部分为编译 dts 文件的核心，其中:  
$(DTC) 表示 dtc 编译器 
$(2) 为 dtb，-O dtb 表示输出文件格式为 dtb 。
-o $@, $@ 为目标文件，表示输出目标文件，输入文件则是对应的 $<。


****  


#### $(obj-m)
\$(obj-m) 是所有需要编译的外部模块列表，一番搜索后并没有发现直接针对 \$(obj-m) 的编译规则，由于它是一系列的 .o 文件，所以它的编译是这样的：
```
$(obj)/%.o: $(src)/%.c $(recordmcount_source) $(objtool_dep) FORCE
	$(call cmd,force_checksrc)
	$(call if_changed_rule,cc_o_c)
```
与上述不同的是，这里使用的是 if_changed_rule 函数，所以继续找到 rule_cc_o_c 指令：
```
define rule_cc_o_c
	$(call cmd,checksrc)
	$(call cmd_and_fixdep,cc_o_c)
	$(call cmd,gen_ksymdeps)
	$(call cmd,checkdoc)
	$(call cmd,objtool)
	$(call cmd,modversions_c)
	$(call cmd,record_mcount)
endef
```

这里又涉及到了 cmd 函数，简单来说，cmd 函数其实就是执行 cmd_$1,也就是上述命令中分别执行 cmd_checksrc,cmd_gen_ksymdeps等等，其中最重要的指令就是 ： $(call cmd_and_fixdep,cc_o_c)，它对目标文件执行了 fixdep，生成依赖文件，然后执行了 cmd_cc_o_c,这个命令就是真正的编译指令:
```
cmd_cc_o_c = $(CC) $(c_flags) -c -o $@ $<
```
不难看出，这条指令就是将所有依赖文件编译成目标文件。  


#### \$(modorder-target)

modorder-target同样是在本文件中定义的，它的值为 $(obj)/modules.order，也就是大多数目录下都存在这么一个 modules.orders 文件，来提供一个该目录下编译模块的列表。
先找到 $(modorder-target) 的编译规则。
```
$(modorder-target): $(subdir-ym) FORCE
	$(Q)(cat /dev/null; $(modorder-cmds)) > $@
```

然后找到 $(modorder-cmds) 的定义
```
modorder-cmds =						\
	$(foreach m, $(modorder),			\
		$(if $(filter %/modules.order, $m),	\
			cat $m;, echo kernel/$m;))
```
从源码可以看出，该操作的目的就是将需要编译的 .ko 的模块以 kernel/$(dir)/*.ko 为名记录到 obj-y/m 指定的目录下。  


#### $(subdir-ym)

在 Makefile.lib 中，这个变量记录了所有在 obj-y/m 中指定的目录部分，同样在本文件中可以找到它的定义：
```
$(subdir-ym):
	$(Q)$(MAKE) $(build)=$@ need-builtin=$(if $(findstring $@,$(subdir-obj-y)),1)
```

我觉得这是整个Makefile.build中最重要的一部分了，因为它解决了我一直以来的一个疑惑，Kbuild系统到底是以怎样的策略递归进入每个子目录中的？

对于每个需要递归进入编译的目录，都对其调用：
```
$(Q)$(MAKE) $(build)=$@ need-builtin=$(if $(findstring $@,$(subdir-obj-y)),1)
```
也就是递归地进入并编译该目录下的文件，基本上，大多数子目录下的 Makefile 并非起编译作用，而只是添加文件。所以，大多数情况下，都是执行了Makefile.build 中的 __build 默认目标进行编译。所以，当我们编译某个目录下的源代码，例如 drivers/ 时，主要的步骤大概是这样的：
1. 由top makefile 调用 $(build) 指令，此时，obj = drivers/ 
2. 在 makefile.build 中包含drivers/Makefile，drivers/Makefile 中的内容为：添加需要编译的文件和需要递归进入的目录，赋值给obj-y/m，并没有定义编译目标，所以默认以 makefile.build 中 __build 作为编译目标。  
3. makefile.build 包含 makefile.lib ,makefile.lib 对 drivers/Makefile 中赋值的 obj-y/m 进行处理，确定 real-obj-y/m subdir-ym 的值。
4. 回到 makefile.build ，执行 __build 目标，编译 real-obj-y/m 等一众目标文件
5. 执行 $(subdir-ym) 的编译，递归地进入 subdir-y/m 进行编译。
6. 最后，直到没有目标可以递归进入，在递归返回的时候生成 built-in.a 文件，每一个目录下的 built-in.a 静态库都包含了该目录下所有子目录中的 built-in.a 和 *.o 文件 ，所以，在最后统一链接的时候，vmlinux.o 只需要链接源码根目录下的几个主要目录下的 built-in.a 即可，比如 drivers, init. 

#### $(always)
相对来说，\$(always)的出场率并不高，而且不会被统一编译，在寥寥无几的几处定义中，一般伴随着它的编译规则，这个变量中记录了所有在编译时每次都需要被编译的目标。  


好了，关于linux内核Kbuild中 scripts/makefile.build 文件的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.



