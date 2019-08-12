# makefile.include详解
在整个Kbuild系统中，scripts/Makefile.include 提供了大量通用函数以及变量的定义，这些定义将被Makefile.build 、Makefile.lib 和 top makefile频繁调用，以实现相应的功能。   

在研究整个Kbuild系统前，有必要先了解这些变量及函数的使用，才能更好地理解整个内核编译的过程。  

下面我们就来看看一些常用且重要的函数及变量定义。  


## $(build)
如果说重要性，这个函数的定义是当之无愧的榜首，我们先来看看它的定义：
```
build := -f $(srctree)/scripts/Makefile.build obj
```
乍一看好像也没啥，我们需要将它代入到具体的调用中才能详细分析，下面举几个调用的示例：
```
%config: scripts_basic outputmakefile FORCE
	$(Q)$(MAKE) $(build)=scripts/kconfig $@

$(vmlinux-dirs): prepare
	$(Q)$(MAKE) $(build)=$@ need-builtin=1
```
其中，第一个是 make menuconfig 的规则，执行配置，生成.config。

第二个是直接执行 make，编译vmlinux的依赖vmlinux-dirs时对应的规则。  


### 示例讲解
以第一个示例为例，将$(build)展开就是这样的：
```
%config: scripts_basic outputmakefile FORCE
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.build obj=scripts/kconfig $@
```
\$(Q)是一个打印控制参数，决定在执行该指令时是否将其打印到终端。
\$(MAKE)，为理解方便，我们可以直接理解为 make。

所以，当我们执行make menuconfig时，就是对应这么一条指令：
```
menuconfig: scripts_basic outputmakefile FORCE
	make -f $(srctree)/scripts/Makefile.build obj=scripts/kconfig menuconfig
```

### 类似指令
与 \$(build) 相类似的，还有以下的编译指令：

#### $(modbuiltin)
```
modbuiltin := -f $(srctree)/scripts/Makefile.modbuiltin obj
```

#### $(dtbinst)
```
dtbinst := -f $(srctree)/scripts/Makefile.dtbinst obj
```

#### $(clean)
```
clean := -f $(srctree)/scripts/Makefile.clean obj
```

#### $(hdr-inst)
```
hdr-inst := -f $(srctree)/scripts/Makefile.headersinst obj
```

这对应的四条指令，分别对应 內建模块编译、dtb文件安装、目标清除和头文件安装。

和\$(build)用法一致，都遵循这样的调用形式：
```
$(Q)$(MAKE) $(build)=$(dir) $(target)
```
至于具体的实现，各位朋友可以自行研究。

****

#### make -f \$(srctree)/scripts/Makefile.build： 
在makefile的规则中，make -f file，这个指令会将file作为makefile读入，相当于先执行另一个指定的文件，这个文件不一定要以makfile命名，在这里就是执行\$(srctree)/scripts/Makefile.build文件。

#### obj=scripts/kconfig 
给目标makfile中obj变量赋值为scripts/kconfig，具体的处理在scripts/Makefile.build中。  

事实上，在scripts/Makefile.build的处理中，会在 obj 所对应的目录中寻找Kbuild 或者 Makefile文件，并包含它。  

因为包含的语句在 scripts/Makefile.build 第一个有效目标之前，所以当不指定make的目标时，将执行被包含的Kbuild 或者 Makefile 中的默认目标，如果没有 Kbuild 或者 Makefile 文件或者文件中不存在目标，就会执行 scripts/Makefile.build 中的默认目标 __build。  

在上述 menuconfig：的示例中，就是先执行scripts/kconfig/Makefile，然后执行该Makefile中指定的目标：menuconfig。  

#### menuconfig 
指定编译的目标，这个目标不一定存在，如果不指定目标，就执行默认目标，在makefile的规则中是makefile中第一个有效目标。

****

### \$(build)功能小结
\$(build)的功能就是将编译的流程转交到对应的文件夹下，鉴于Kbuild系统的兼容性和复杂性。

\$(build)起到一个承上启下的作用，从上接收编译指令，往下将指令分发给对应的功能部分，包括配置、源码编译、dtb编译等等。  

**** 

## $(if_changed)
if_changed 指令也是当仁不让的核心指令，顾名思义，它的功能就是检查文件的变动。  

在Makefile中实际的用法是这样的,
```
foo:
    $(call if_changed,$@)
```

一般情况下if_changed函数被调用时，后面都接一个参数，那么这个语句是什么作用呢？我们来看具体定义：
```
if_changed = $(if $(strip $(any-prereq) $(arg-check)),                       \
	$(cmd);                                                              \
	printf '%s\n' 'cmd_$@ := $(make-cmd)' > $(dot-target).cmd, @:)
```
首先，是一个 if 函数判断，判断 \$(any-prereq) \$(arg-check)是否都为空，如果两者有一者不为空，则执行$(cmd),否则打印相关信息。  

其中，any-prereq 表示依赖文件中被修改的文件。  

arg-check，顾名思义，就是检查传入的参数，检查是否存在 cmd_$@,至于为什么是cmd_$@，我们接着看。  

### any-prereq || arg-check 不为空
当any-prereq || arg-check不为空时，就执行\$(cmd)，那么 \$(cmd) 是什么意思呢？
```
echo-cmd = $(if $($(quiet)cmd_$(1)),\
	echo '  $(call escsq,$($(quiet)cmd_$(1)))$(echo-why)';)
cmd = @set -e; $(echo-cmd) $(cmd_$(1))
```

可以看到，cmd第一步执行echo-cmd，这个指令就是根据用户传入的控制参数，决定是否打印当前命令。  

然后，就是执行cmd_$(1)，$(1) 是 if_changed 执行时传入的参数，所以在这里相当于执行 cmd_foo。  

另一个分支就是echo '  $(call escsq,$($(quiet)cmd_$(1)))$(echo-why)';)，这是打印结果到文件中，并不自行具体的编译动作。  

### $(if_changed)使用小结
简单来说，调用 if_changed 时会传入一个参数$var，当目标的依赖文件有更新时，就执行 cmd_$var 指令。比如下面vmlinux编译的示例：
```
cmd_link-vmlinux =                                                 \
	$(CONFIG_SHELL) $< $(LD) $(KBUILD_LDFLAGS) $(LDFLAGS_vmlinux) ;    \
	$(if $(ARCH_POSTLINK), $(MAKE) -f $(ARCH_POSTLINK) $@, true)

vmlinux: scripts/link-vmlinux.sh autoksyms_recursive $(vmlinux-deps) FORCE
	$(call if_changed,link-vmlinux)
```
可以看到，vmlinux 的依赖文件列表为 scripts/link-vmlinux.sh autoksyms_recursive $(vmlinux-deps) FORCE。  

调用 if_changed 函数时传入的参数为 link-vmlinux，当依赖文件有更新时，将执行 cmd_link-vmlinux 。

### 同类型函数
与$(if_changed)同类型的，还有$(if_changed_dep),$(if_changed_rule)。  
```
if_changed_dep = $(if $(strip $(any-prereq) $(arg-check)),$(cmd_and_fixdep),@:)
if_changed_rule = $(if $(strip $(any-prereq) $(arg-check)),$(rule_$(1)),@:)
```
if_changed_dep 与 if_changed 同样执行cmd_$1,不同的是它会检查目标的依赖文件列表是否有更新。   


需要注意的是，这里的依赖文件并非指的是规则中的依赖，而是指fixdep程序生成的 .*.o.cmd 文件。  

而 if_changed_rule 与 if_changed 不同的是，if_changed 执行 cmd_$1,而 if_changed_rule 执行 rule_$1 指令。

****  

## $(filechk)
同样，通过名称就可以看出，它的功能是检查文件，具体来说就是先操作文件，再检查文件的更新。  

它的定义是这样的：
```
define filechk
	$(Q)set -e;				\
	mkdir -p $(dir $@);			\
	{ $(filechk_$(1)); } > $@.tmp;		\
	if [ -r $@ ] && cmp -s $@ $@.tmp; then	\
		rm -f $@.tmp;			\
	else					\
		$(kecho) '  UPD     $@';	\
		mv -f $@.tmp $@;		\
	fi
endef
```
它主要实现的操作是这样的：
1. mkdir -p $(dir $@)：如果$@目录不存在，就创建目录，$@是编译规则中的目标部分。
2. 执行 filechk_$(1) ,然后将执行结果保存到$@.tmp中
3. 对比 $@.tmp 和 $@ 是否有更新，有更新就使用 $@.tmp，否则删除 $@.tmp。

说白了，\$(filechk) 的作用就是执行 filechk_$@ 然后取最优的结果。  

举一个 top makefile 中的例子：
```
filechk_kernel.release = \
	echo "$(KERNELVERSION)$$($(CONFIG_SHELL) $(srctree)/scripts/setlocalversion $(srctree))"  

include/config/kernel.release: FORCE
	$(call filechk,kernel.release)
```
这部分命令的作用就是输出 kernelrelease 到 kernel.release.tmp 文件，最后对比 kernel.release 文件是否更新，kernelrelease对应内核版本。  

同时，如果include/config/不存在，就创建该目录。  

****   








