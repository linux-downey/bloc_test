# top makefile 详解 - 内核版本的生成
经过一系列的铺垫，我们开始研究 top makefile(在下文中统一称为makefile)。   

由于各内核版本的 makefile 实际上有一些位置的调整和功能的小区别，这里就不逐行地对makefile进行分析，而是以功能为单位，各位可以对照自己研究的makefile版本找到对应的部分，框架部分是完全通用的。  


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
检查 auto.conf文件是否存在，auto.conf文件是在配置阶段被生成的，也就是 make *config。  

****  

再往下看：
```
res="$(collect_files localversion*)"
if test ! "$srctree" -ef .; then
	res="$res$(collect_files "$srctree"/localversion*)"
fi
```

