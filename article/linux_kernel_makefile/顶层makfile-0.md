# top makefile 详解 - 内核版本的生成



## 内核版本的定义

在 top makefile 开头，就定义了当前的内核版本：
```
VERSION = 5
PATCHLEVEL = 2
SUBLEVEL = 0
EXTRAVERSION = -rc4
```

在接下来的 makefile 中，发现被赋值给了 KERNELRELEASE 和 KERNELVERSION 两个变量，并 export 两个变量到其子makefile中。  

```
KERNELRELEASE = $(shell cat include/config/kernel.release 2> /dev/null)
KERNELVERSION = $(VERSION)$(if $(PATCHLEVEL),.$(PATCHLEVEL)$(if $(SUBLEVEL),.$(SUBLEVEL)))$(EXTRAVERSION)
export VERSION PATCHLEVEL SUBLEVEL KERNELRELEASE KERNELVERSION
```

KERNELRELEASE 表示发布版本，而 KERNELVERSION 表示内核的版本。  

在一个开发板上， 我们使用 uname -r 看到的就是发布版本，这个版本将会在很多地方用到。  

****  

## 实际开发中发布版本的问题
在实际的开发中，经常碰到一个非常恼火的问题，在使用 git 管理内核源码时，本来从 git 仓库 clone 下来的源码编译得好好的，版本也是没有问题。   

但是，稍微改动一下源码，再编译出来的镜像就不对劲了，在该镜像运行的系统下，使用uname -r 命令查询到的版本号往往是这样的：  
```
5.2.0-rc4+
```
从而导致模块不能正常加载，外部模块的编译也不能正常运行，因为 modprobe 加载模块默认是加载 /lib/module/`uname -r`/ 目录下的，之前文件系统中建立的是 /lib/module/5.2.0-rc4/ 目录，现在平白无故多了个 '+' ，自然是找不到相应目录的。  

而外部模块的编译通常也是依赖 /usr/src/`uname -r`,`uname -r`的结果变了，自然就不能正常工作了。  

linux中开发的一大好处就是，一切都是透明的，只要你愿意深入源代码，问题终将会浮出水面。  

****  

## 内核版本的生成
makefile 支持命令行参数查询该源码最后编译将得到的发布版本，在编译之前，我们可以通过以下命令事先确定内核编译的发布版本：  
```
make kernelrelease
```
在内核镜像的编译时，也依赖 kernelrelease ，说明内核发行版本号就是由这个命令生成的，既然找到了问题的源头，那么就是顺着这个线头找下去了。  


## kernelrelease 定义
首先，既然可以直接执行 make kernelrelease，那么在 makefile 中肯定能找到 kernelrelease 这个目标的定义：

```
@echo "$(KERNELVERSION)$$($(CONFIG_SHELL) $(srctree)/scripts/setlocalversion $(srctree))"
```

其中，KERNELVERSION 变量根据 makefile 在文件开头处的定义而生成，是一个固定的字符串 ：  5.2.0-rc4 .

CONFIG_SHELL 可以直接理解为 /bin/shell

那么，可以肯定的是，kernelrelease 的值由 $(srctree)/scripts/setlocalversion 这个脚本决定。  

所以，对应的发型版本号为 ： $(KERNELVERSION) + 脚本输出的结果

接下来就需要分析脚本输出结果。  

## setlocalversion 脚本
在上一节中，\$(srctree)/scripts/setlocalversion 脚本被执行，传入的参数为 \$(srctree), srctree 为源码树的根目录。  

分析 setlocalversion 脚本中的内容,略过函数定义部分，被调用的时候再贴出来，我们先从命令执行的部分开始：
```
if $scm_only; then
	if test ! -e .scmversion; then
		res=$(scm_version)
		echo "$res" >.scmversion
	fi
	exit
fi
```
从 scm_only 可以看出，这里的处理是针对 scm 版本管理的，暂时不用理会，因为我们的目标是 git 。  

****  

接下来：
```
if test -e include/config/auto.conf; then
	. include/config/auto.conf
else
	echo "Error: kernelrelease not valid - run 'make prepare' to update it" >&2
	exit 1
fi
```
检查 auto.conf文件是否存在，auto.conf文件是在配置阶段被生成的，也就是 make *config,如果不存在 auto.conf 文件，就报错。  

****  

再往下看：
```
res="$(collect_files localversion*)"
if test ! "$srctree" -ef .; then
	res="$res$(collect_files "$srctree"/localversion*)"
fi
```
检查源码中是否存在 localversion* 文件，如果存在，则取文件中的版本号。一般情况下，虽然存在这么一个接口，但是却很少用这个接口来生成版本号。  

****

```
res="${res}${CONFIG_LOCALVERSION}${LOCALVERSION}"
```

将 $(CONFIG_LOCALVERSION) $(LOCALVERSION) 添加到版本号中，CONFIG_LOCALVERSION 被定义在 .config 文件中，默认情况下为一个空字符串，而 CONFIG_LOCALVERSION 默认没有被定义，我们同样可以通过控制这两个变量来修改版本号的值。

****  
```
if test "$CONFIG_LOCALVERSION_AUTO" = "y"; then
	# full scm version string
	res="$res$(scm_version)"
else
	if test "${LOCALVERSION+set}" != "set"; then
			scm=$(scm_version --short)
			res="$res${scm:++}"
		fi
fi
```
这一部分是整个版本号配置过程的核心逻辑部分，首先，判断 CONFIG_LOCALVERSION_AUTO 是否为 y，这个变量同样是在 .config 文件中被定义，默认情况下这个变量是没有被配置的。  

如果对其进行了配置，就会调用 scm_version 函数。

如果是默认情况下，没有对其进行配置，那就执行另一个分支，显示检查 LOCALVERSION 是否为空(即未被设置或定义)，如果不为空，则结束循环。如果为空，那么同样调用 scm_version 函数，并传入 --short 参数。  

既然两个分支都是调用 scm_version 函数，我们就来看看这个函数的大致实现：
```
if test "$1" = "--short"; then
		short=true
	fi
```
首先，根据是否有 --short 参数来对 short 变量进行赋值。  
```
1 if test -z "$(git rev-parse --show-cdup 2>/dev/null)" &&
2 	head=`git rev-parse --verify --short HEAD 2>/dev/null`; then
3 		
4 		if [ -z "`git describe --exact-match 2>/dev/null`" ]; then
5 			if $short; then
6 				echo "+"
7 				return
8 			fi
9 		fi
10 		...
11 	fi
```
* 第 1 行 ：git 的 rev-parse --show-cdup 指令是判断当前目录位于git仓库的深度，test -z 表示检查返回的字符串是否为空。也就是，只有当前目录树存在git仓库 且 在git仓库根目录下，该条件成立。
* 第 2 行：git 的 rev-parse --verify --short HEAD 指令是找到输出当前 HEAD 指针的值，确定其不为空
* 第 4 行： 满足 1~2 行的条件下，使用 git describe --exact-match 2>/dev/null 指令查找精确匹配，也就是说，如果当前分支是一个tag且直接指向一个提交，且没有被修改过，该命令就输出对应的匹配，否则报错。  
	但是，该命令最后使用 2>/dev/null 忽略错误信息，所以整个 if [ -z "`git describe --exact-match 2>/dev/null`" ]; then 表示：如果 --exact-match 匹配失败(通常情况下就是人为地修改了源码，但是没有提交)，就执行 if 条件下的语句
* 5 ~ 8 行：在上一步 --exact-match 匹配失败的情况下，如果 short 不为空 ，就输出 "+" ,在这里，我们就看到了在内核发行版后添加 "+" 的罪魁祸首。  

进一步梳理一下，在内核发行版添加 "+" 的条件是这样的：
1. CONFIG_LOCALVERSION_AUTO 变量未在.config中被设置为y
2. LOCALVERSION 变量未被定义,注意是未被定义，不是定义为空
3. 当前的 git 分支不是一个指向一个commit的tag或者被改动


## 问题的解决方案
既然问题出现的流程已经梳理清楚了，那么思路就是破坏它产生的前提条件就好了

### 设置 CONFIG_LOCALVERSION_AUTO
CONFIG_LOCALVERSION_AUTO 控制发型版本号的后缀，但是它也会根据 git 提供的信息添加后缀，如果你没有严格地按照git提供的方式管理管理源代码，它也会自动添加关于版本管理的后缀信息，类似于这样：
```
5.2.0--02301-gfa8b95b363c1-dirty
```
-dirty 后缀表示修改了代码没有提交。   

### 设置 LOCALVERSION 变量
第二个办法就是设置 LOCALVERSION 变量，这个方法在我看来是最方便的，在上述的脚本中，如果设置了 LOCALVERSION 变量，即使是为空，也不会调用到 scm_version 函数。  

这样的情况下，内核发型版本就完全由三个部分决定：
* KERNELVERSION : 这是被定义在 top makefile 头部版本信息所决定的
* CONFIG_LOCALVERSION : 默认在 .config 文件中为空，我们可以由它来指定对应的内核发行版
* LOCALVERSION : 这个变量必须被设置，可以直接在编译时命令行传入，为了避免添加意外地被添加 "+"，设置成空也是可以的。  


## 实际使用
在实际的开发中，每个厂商发布的内核镜像版本总会带着一些自定义的字段，比如 ti 的内核发型版就像这样：
```
4.14.79-ti-r100
```
KERNELVERSION 被设置为 4.14.79
后缀部分就是 -ti-r100，

我们可以在 .config 文件中将 CONFIG_LOCALVERSION 设置为 "-ti-r100",然后在编译时命令行传入 LOCALVERSION=""即可，或者是直接传入LOCALVERSION="-ti-r100"，而不修改 CONFIG_LOCALVERSION 变量。这两种情况下编译出来的镜像发行版本都将是：
```
4.14.79-ti-r100
```
而不是讨厌的：
```
4.14.79-ti-r100+
```




