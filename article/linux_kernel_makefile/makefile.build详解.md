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


我们将其拆开来看：

builtin-target ： 在上文中可以看到，这个变量的值为：\$(obj)/built-in.a，也就是当前目录下的built-in.a文件，这个文件在源文件中自然是没有的，需要编译生成，我们在后文中寻找它的生成规则：


可以找到这样一条：
```
$(builtin-target): $(real-obj-y) FORCE
	$(call if_changed,ar_builtin)
```

#### if_changed解析



#### 第一部分

```
$(if $(KBUILD_BUILTIN),$(builtin-target) $(lib-target) $(extra-y))
```
这是一个if函数，如果KBUILD_BUILTIN为真，则添加\$(builtin-target) \$(lib-target) \$(extra-y)三个目标文件，如果为假，则不执行任何命令。  


#### 第二部分
```
$(if $(KBUILD_MODULES),$(obj-m) $(modorder-target))
```
同样的，这也是一个if函数，如果KBUILD_MODULES为真，则添加\$(obj-m) \$(modorder-target)目标文件，如果为假，则不执行任何命令。

#### 第三部分
相对而言，这一部分较简单，添加\$(subdir-ym)和\$(always)，\$(subdir-ym)是一个目录列表,记录了需要递归进入的目录(包含外部模块和內建模块)。  











