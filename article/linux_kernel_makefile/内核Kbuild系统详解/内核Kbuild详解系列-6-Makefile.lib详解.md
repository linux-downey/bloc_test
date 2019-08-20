# linux Kbuild详解系列(6)-scripts/Makefile.lib 文件详解

在linux内核的整个Kbuild系统中，Makefile.lib 对于目标编译起着决定性的作用，如果说 Makefile.build 负责执行 make 的编译过程，而 Makefile.lib 则决定了哪些文件需要编译，哪些目录需要递归进入。  

接下来我们就来看看他们的具体实现。  

***  

## 源文件从哪里来
首先，既然是对编译文件进行处理，那么，第一个要处理的问题是：源文件从哪里来？  

进入到各级子目录下,可以发现 Makefile 内容，我们以最熟悉的 drvier/i2c/Makefile 为例：
```
....
obj-$(CONFIG_I2C_CHARDEV)   += i2c-dev.o
obj-$(CONFIG_I2C_MUX)       += i2c-mux.o
obj-y               += algos/ busses/ muxes/
obj-$(CONFIG_I2C_STUB)      += i2c-stub.o
obj-$(CONFIG_I2C_SLAVE_EEPROM)  += i2c-slave-eeprom.o
....
```

可以发现，一般的形式都是 
```
obj-$(CONFIG_VAR) += var.o
```
这个 CONFIG_VAR 在 make menuconfig 的配置阶段被设置，被保存在.config 中，对应的值根据配置分别为：y , m 或 空。  

对于某些系统必须支持的模块，可以直接写死在 obj-y 处。  

还有一个发现就是：obj-y 或 obj-m 对应的并不一定是目标文件，也可以是目标目录，这些目录同样会根据配置选项选择是否递归进入。  

对于分散在各个目录下的Makefile，基本上都是这么个形式(除了arch/下的较特殊)。

***  

## Makefile.lib内容解析
接下来我们直接打开 scripts/Makefile.lib 文件，逐句地对其进行分析。  

### 编译标志位
Makefile.lib开头的部分是这样的：
```
asflags-y  += $(EXTRA_AFLAGS)
ccflags-y  += $(EXTRA_CFLAGS)
cppflags-y += $(EXTRA_CPPFLAGS)
ldflags-y  += $(EXTRA_LDFLAGS)

KBUILD_AFLAGS += $(subdir-asflags-y)
KBUILD_CFLAGS += $(subdir-ccflags-y)
```
这些大多是一些标志位的设置，细节部分我们就不关注了，我们只关注框架流程部分。  

***  

### 目录及文件处理部分

#### 去除重复部分

```
// 去除obj-m中已经定义在obj-y中的部分
obj-m := $(filter-out $(obj-y),$(obj-m))
// 去除lib-y中已经定义在obj-y中的部分
lib-y := $(filter-out $(obj-y), $(sort $(lib-y) $(lib-m)))
```
这一部分主要是去重，如果某个模块已经被定义在obj-y中，证明已经或者将要被编译进内核，就没有必要在其他地方进行编译了。  

****  

#### modorder处理
```
//将obj-y中的目录 dir 修改为 dir/modules.order赋值给modorder，
//将obj-m中的.o修改为.ko赋值给modorder。 
modorder	:= $(patsubst %/,%/modules.order,\
                 $(filter %/, $(obj-y)) $(obj-m:.o=.ko))
```

从 modorder 的源码定义来看，将 obj-y/m 的 %/ 提取出来并修改为 %/modules.order,比如 obj-y 中的 driver/ 变量，则将其修改为 driver/modules.order 并赋值给 modorder 变量。  

同时，将所有的 obj-m 中的 .o 替换成 .ko 文件并赋值给 modorder。

那么，各子目录下的 modules.order 文件有什么作用呢？

官方文档解析为：This file records the order in which modules appear in Makefiles. Thisis used by modprobe to deterministically resolve aliases that matchmultiple modules。

内核将编译的外部模块全部记录在 modules.order 文件中，以便 modprobe 命令在加载卸载时查询使用。    

**** 

#### 目录的处理
在查看各级子目录下的 Makefile 时，发现 obj-y/m 的值并非只有目标文件，还有一些目标文件夹，但是文件夹并不能被直接编译，那么它是被怎么处理的呢？带着这个疑问接着看：

```
//挑选出obj-y 和 obj-m 中的纯目录部分，然后添加到subdir-y和subdir-m中。  
__subdir-y	:= $(patsubst %/,%,$(filter %/, $(obj-y)))
subdir-y	+= $(__subdir-y)

__subdir-m	:= $(patsubst %/,%,$(filter %/, $(obj-m)))
subdir-m	+= $(__subdir-m)

//需要被递归搜寻的子路径，带有可编译内部和外部模块的子目录。  
subdir-ym	:= $(sort $(subdir-y) $(subdir-m))

//obj-y 中纯目录部分则将其改名为dir/build-in.a,obj-y的其他部分则不变。  
obj-y		:= $(patsubst %/, %/built-in.a, $(obj-y))

//将obj-m中的纯目录部分剔除掉(因为已经在上面加入到subdir-m中了)。
obj-m		:= $(filter-out %/, $(obj-m))

```
在上文中提到，obj-y 和 obj-m 的定义中同时夹杂着目标文件和目标文件夹，文件夹当然是不能直接参与编译的，所以需要将文件夹提取出来。  

具体来说，就是将 obj-y/m 中以"/"结尾的纯目录部分提取出来，并赋值给 subdir-ym。 

最后，在处理完后，需要删除这一部分，对于obj-y的目录而言，将其修改成 dir/build-in.a(其实就是删除 obj-y 中目录再添加一个 build-in.a 的目标文件)，而对于obj-m中的目录，则是直接删除。

***

#### 多文件依赖的处理
```
//这里的理解难点为：先将obj-y中的每个元素从 xxx.o 转换成 xxx-objs,然后再检查 $(xxx-objs)是否存在，如果存在，那么这个
//模块就即在obj-y里存在，又在xxx-objs/y/m中存在，赋值给multi-used-xxx。  
multi-used-y := $(sort $(foreach m,$(obj-y), $(if $(strip $($(m:.o=-objs)) $($(m:.o=-y))), $(m))))
multi-used-m := $(sort $(foreach m,$(obj-m), $(if $(strip $($(m:.o=-objs)) $($(m:.o=-y)) $($(m:.o=-m))), $(m))))
multi-used   := $(multi-used-y) $(multi-used-m)

仅仅是用作外部模块的目标。  
single-used-m := $(sort $(filter-out $(multi-used-m),$(obj-m)))
```
这里处理的是一个目标文件是否依赖于多个目标文件的问题。   

这一部分的理解是比较难的，需要有一定的Makefile功底，具体实现如下：

对于obj-y/m中每一个元素，查找对应的xxx-objs是否存在。不知道你是否还记得前面章节所说的，如果 foo.o 目标依赖于 a.c,b.c,c.c,那么它在 Makefile 中的写法是这样的：
```
obj-y += foo.o
foo-objs = a.o b.o c.o
```

所以对于foo-y,系统同时查找 foo-objs，如果存在，就表示该模块并非依赖单一的foo.c 文件，而是依赖多个文件进行编译。  

可以直接看注释，最后实现的结果就是将这些文件分成被多重依赖和单独依赖，并被赋值给 multi-used 和 single-used-m。

*** 

#### 编译的目标文件处理

接下来的部分，就属于真正的目标文件部分了。  

```
//obj-y中的纯目录，在上面被修改为xxx/built-in.a  在这里赋值给 subdir-obj-y
subdir-obj-y := $(filter %/built-in.a, $(obj-y))


//这里的 obj-y 剔除了纯目录部分. xxx.o与 xxx-objs/y同时存在时，将xxx.o 转换为xxx-objs,//xxx-y obj-m 同理，这些都是目标文件，而不存在目录。
real-obj-y := $(foreach m, $(obj-y), $(if $(strip $($(m:.o=-objs)) $($(m:.o=-y))),$($(m:.o=-objs)) $($(m:.o=-y)),$(m)))
real-obj-m := $(foreach m, $(obj-m), $(if $(strip $($(m:.o=-objs)) $($(m:.o=-y)) $($(m:.o=-m))),$($(m:.o=-objs)) $($(m:.o=-y)) $($(m:.o=-m)),$(m)))

```
首先，将所有的$(dir)/built-in.a 赋值给 subdir-obj-y，这一部分将在每个模块文件夹下编译完成之后链接成 built-in.a 库。  

所以，在编译完成的linux源代码下，大多数目录下都存在一个 built-in.a 文件。

在上文中，将所有的 obj-y 和 obj-m 剔除掉目录之后，就剩下了所有需要编译的目标文件，从名字 real-obj-y/m 可以看出，这是真正需要编译的目标文件。  

有些朋友不禁要问了，那那些 obj-y/m 中的目录怎么处理？  

答案是：在上文中，那些目录被赋值给了subdir-ym，将在Makefile.build文件中执行编译时，递归地进入各级目录进行编译，可以参考后续的章节中对 Makefile.build 的详解部分。  

****  

#### 设备树相关
```
extra-y				+= $(dtb-y)
extra-$(CONFIG_OF_ALL_DTBS)	+= $(dtb-)

ifneq ($(CHECK_DTBS),)
extra-y += $(patsubst %.dtb,%.dt.yaml, $(dtb-y))
extra-$(CONFIG_OF_ALL_DTBS) += $(patsubst %.dtb,%.dt.yaml, $(dtb-))
endif
```
Kbuild系统同时支持设备树的编译，除了编译器使用的是dtc，其他编译操作几乎是一致的。  

将所有的 dtb-y 赋值给extra-y，并对设备树文件进行相应检查。  

****  

#### 添加路径
```
extra-y		:= $(addprefix $(obj)/,$(extra-y))
always		:= $(addprefix $(obj)/,$(always))
targets		:= $(addprefix $(obj)/,$(targets))
modorder	:= $(addprefix $(obj)/,$(modorder))
obj-m		:= $(addprefix $(obj)/,$(obj-m))
lib-y		:= $(addprefix $(obj)/,$(lib-y))
subdir-obj-y	:= $(addprefix $(obj)/,$(subdir-obj-y))
real-obj-y	:= $(addprefix $(obj)/,$(real-obj-y))
real-obj-m	:= $(addprefix $(obj)/,$(real-obj-m))
single-used-m	:= $(addprefix $(obj)/,$(single-used-m))
multi-used-m	:= $(addprefix $(obj)/,$(multi-used-m))
subdir-ym	:= $(addprefix $(obj)/,$(subdir-ym))
```
文件的处理最后，给所有的变量加上相应的路径，以便编译的时候进行索引。  

Makefile中的 addprefix 函数指添加前缀.  

至于上述添加路径部分的 $(obj) 是从哪里来的呢？  
事实上，Makefile.lib 通常都被包含在于 Makefile.build中，这个变量继承了 Makefile.build 的 obj 变量。

而 Makefile.build 的 obj 变量则是通过调用 \$(build) 时进行赋值的。  

****  

#### 其余部分
基本上，对于文件和目录的处理到这里就已经完成了，Makefile.lib中剩余的部分都是一些变量及标志位的设置，本文旨在梳理框架，对于那些细节部分有兴趣的朋友可以研究研究，这里就不再继续深入了。



好了，关于 # linux Kbuild详解系列(6)-scripts/Makefile.lib文件详解 的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.



