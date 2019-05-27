# linux下deb包的制作(2) -- 源码包和自动编译构建deb包
在上一章节中，我们讨论了一个deb包的制造，演示了从0开始创建一个hello world的二进制debian包。  

## 使用debhelper实现deb包的打包
在这一章中，我们来使用官方提供的自动化构建工具集**debhelper**来创建一个由源码生成的deb包，源码包最大的好处就是它非常不错的跨平台性能。  



## 准备源码包  

### 源代码
第一步还是准备源代码，同样是hello world示例程序，源代码就是所有学C的朋友都写过的hello world。
hello_world.c ：
```
#include <stdio.h>
void main(void)
{
	printf("hello world!\r\n");
}
```
***
### 其他文件  
同样的，为了演示，博主准备在安装时在/etc目录下放置一个测试文件 debhello。  
***  
### Makefile
既然是源码编译，那么肯定需要一个Makefile来指导make的编译：
```
1 all:
2     gcc src/hello_world.c -o src/hello_world
3 clean:
4     rm -rf src/hello_world
5 install:
6     /usr/bin/install -D src/hello_world ${DESTDIR}/usr/bin/hello_world
7     /usr/bin/install -D etc/debhello ${DESTDIR}/etc/debhello
8 
9 uninstall:
10     -rm  ${DESTDIR}/usr/bin/hello_world
11     -rm  ${DESTDIR}/etc/debhello

```

在Makefile文件中，指定编译目标，clean命令、install、uninstall命令，这些目标在程序的编译和安装时会被对应的deb程序调用以实现对应的功能。    
### 源码包打包
自动构建deb包指令需要用户将源码包打包，以$PKGNAME-$VERSION.tar.gz的形式对包进行命名，这个命名形式是严格要求的，这里我们使用tar指令进行打包：
```
tar -czvf debhello-1.0.0.tar.gz debhello-1.0.0/
debhello-1.0.0为原目录名称
```

最后整个源码的目录结果就是这样的，目录结构需要按照严格的要求，不然会出现无法识别或者安装出错的异常情况：
```
.
├── debhello-1.0.0
│   ├── etc
│   │   └── debhello
│   ├── Makefile
│   └── src
│       └── hello_world.c
└── debhello-1.0.0.tar.gz
```


### 自动生成配置文件模板
值得一提的是，与上一章节的制作方法不同，这章所讲的包制作方法并不需要自己创建控制文件(DEBIAN目录以及之下的控制文件，比如control changelog等等文件)。  

我们直接使用以下指令来完成：
```
cd debhello-1.0.0
debmake
```
执行完成之后，会发现整个源码包目录下多了很多配置文件。执行tree查看整个目录的结构：    

```
.
├── debhello-1.0.0
│   ├── debian
│   │   ├── changelog
│   │   ├── compat
│   │   ├── control
│   │   ├── copyright
│   │   ├── patches
│   │   │   └── series
│   │   ├── README.Debian
│   │   ├── rules
│   │   ├── source
│   │   │   ├── format
│   │   │   └── local-options
│   │   └── watch
│   ├── etc
│   │   └── debhello
│   ├── Makefile
│   └── src
│       └── hello_world.c
├── debhello_1.0.0.orig.tar.gz -> debhello-1.0.0.tar.gz
└── debhello-1.0.0.tar.gz

```
其中，debmake指令为我们生成了整个debian目录下的控制文件，还生成了一个软连接debhello_1.0.0.orig.tar.gz指向源码包。  

### debmake纠错
世事总是不会尽如人意，在执行debmake时，很可能你会碰到一些问题，但是linux下报错时你完全根据报错信息来诊断错误并解决：  
  
* 第一个问题：
```
I: set parameters
I: sanity check of parameters
E: invalid parent directory for setting package/version: debhello_1.0.0
E: rename parent directory to "package-version".
```
这个报错通常是你没有按照目录的标准命名，目录需要使用package-version命名.  

* 第二个问题：
```
I: pkg="debhello", ver="1.0.0", rev="1"
I: *** start packaging in "debhello-1.0.0". ***
I: provide debhello_1.0.0.orig.tar.gz for non-native Debian package
I: pwd = "/home/huangdao/blog_mkdeb"
E: missing "debhello-1.0.0.tar.gz".

```
这个错误是因为你没有将源码进行压缩，或者压缩之后的文件名使用了非标准命名，又或者是压缩文件放置到了错误的目录下。    

* 第三个问题：
```
I: set parameters
Traceback (most recent call last):
  File "/usr/bin/debmake", line 28, in <module>
    debmake.main()
  File "/usr/lib/python3/dist-packages/debmake/__init__.py", line 104, in main
    para = debmake.para.para(para)
  File "/usr/lib/python3/dist-packages/debmake/para.py", line 44, in para
    debmail = os.getlogin() + '@localhost'
FileNotFoundError: [Errno 2] No such file or directory
```
这个问题事实上是python的一个bug，在某些python版本会出现，原因是无法获取用户名。解决方案有两个：
1、设置环境变量，主动设置自己的用户信息，也就是包的作者信息，就不需要去通过python脚本获取用户名。  
```
export DEBEMAIL="linux_downey@gmail.com"
export DEBFULLNAME="downey_boy"
```
2、找到脚本出错位置，然后修改它，对比第一种方案而言，这是治本的办法，首先根据提示找到 **/usr/lib/python3/dist-packages/debmake/para.py**出错未见的44行，既然这条语句有问题，那我们就将其换一种获取用户名的方法：
```
import os, pwd
os.getlogin = lambda: pwd.getpwuid(os.getuid())[0]
```
问题基本就解决了。  



## debmake生成的文件解析
在以上步骤完成后，debmake工具仅仅是为我们生成标准的文件列表，具体的控制文件内容还是需要我们来编辑。    
先使用**ls**指令查看一下自动生成的debian目录下有哪些文件：
```
changelog  control    patches   postrm         rules   watch
compat     copyright  postinst  README.Debian  source
```
在这些文件当中，changelog、control、copyright、postrm、postinst的应用以及修改在上一章节已经详细介绍过，这里就不再赘述，我们主要看看其他的文件
### compat
非必须，compat事实上是compatible的缩写，表示debhelper的兼容级别。通常为9或者10，在使用版本10的时候也可以选择兼容旧版本9.  

### patches
非必须，这是一个目录文件，存放一个包的补丁文件，全新的 3.0 (quilt) 源代码格式将所有修改使用 quilt 补丁系列记录到 debian/patches。这些修改会在解压源代码包时自动应用。  

### README.Debian
非必须，所有关于你的 Debian 版本与原始版本间的额外信息或差别都应在这里记录。  
dh_make 创建了一个默认的文件，以下是它的样子：  
```
gentoo for Debian
-----------------
<possible notes regarding this package - if none, delete this file>
 -- Josip Rodin <joy-mg@debian.org>, Wed, 11 Nov 1998 21:02:14 +0100
```
它的编写需要遵循一定的格式，同时，如果你没有需要写在这里的东西，则删除这个文件。  

### source 
非必须，这是一个目录文件，其中包含了一些控制文件，一般属于一些灵活的配置项，这些配置项在某些时候能达到更好的效果，更加适用于应用场景，通常在复杂的应用场景下可以使用。举几个简单的示例：
* source/local-options ： 版本管理系统的选项，比如非常受欢迎的git。  
* source/options ： 在源码树下自动生成的文件有时会超级讨厌，因为它们会生成毫无意义巨大无比的补丁文件。 为了减轻这种问题，可以用一些定制模块比如 dh_autoreconf，你可以给 --extend-diff-ignore 选项提供一个 Perl 正则表达式作为 dpkg-source(1) 的参数，以此忽略 在创建源码包时自动生成的文件所发生的变更。  
* source/format ： 在 debian/source/format 中只包含一行，写明了此源代码包的格式，其中包含两个选项：
  * 3.0 (native) - Debian 本土软件或者
  * 3.0 (quilt) - 其他所有软件


### watch
非必须，watch 文件配置了 uscan 程序(它在 devscripts 中)以便监视你获得原始源代码的网站，这对代码的更新跟踪比较实用。  


### rules 
必须，**这个是用于实际创建软件包的文件，在这一节中我们主要关注这个文件，它在源码编译的时候是必须的**,这个文件事实上是另一个 Makefile，但不同于上游源代码中的那个Makefile。和debian 目录中的其他文件不同，这个文件被标记为可执行文件。    

事实上，自动化生成的默认rules，是可以直接使用的，它的内容是这样的：
```
1 #!/usr/bin/make -f
 2 # See debhelper(7) (uncomment to enable)
 3 # output every command that modifies files on the build system.
 4 #DH_VERBOSE = 1
 5 
 6 # see FEATURE AREAS in dpkg-buildflags(1)
 7 #export DEB_BUILD_MAINT_OPTIONS = hardening=+all
 8
 9 # see ENVIRONMENT in dpkg-buildflags(1)
10 # package maintainers to append CFLAGS
11 #export DEB_CFLAGS_MAINT_APPEND  = -Wall -pedantic
12 # package maintainers to append LDFLAGS
13 #export DEB_LDFLAGS_MAINT_APPEND = -Wl,--as-needed
14 
15
16 %:
17         dh $@ 
```
熟悉shell的都知道，第一行是指定脚本解析方式。  

其中以 **#**开头的行都属于注释部分。   

那么唯一起到实际作用的就是16、17行了。熟悉makefile的朋友应该知道，这种形式属于模式规则的语法，以此隐式地完成所有工作。其中的百分号意味着“任何 targets”， 它会以 target 名称作参数调用单个程序 dh，dh命令是一个包装脚本，它会根据参数执行妥当的 dh_* 程序序列。   

### 使用脚本生成deb包
在所有文件修改完成之后，就可以使用以下指令来编译deb包了：
```
debuild -us -uc
```




### 自定义rules文件
rules文件的规则，语法和Makefile非常像，rules文件中包含了多个规则，一个新的rule以自己的target进行声明来起头，后续的行以TAB开头，以下是对各target的简单解释：  

* clean target：清理所有编译的、生成的或编译树中无用的文件。(必须)
* build target：在编译树中将代码编译为程序并生成格式化的文档。(必须)
* build-arch target：在编译树中将代码编译为依赖于体系结构的程序。(必须)
* build-indep target：在编译树中将代码编译为独立于平台的格式化文档。(必须)
* install target：把文件安装到 debian 目录内为各个二进制包构建的文件树。如果有定义，那么 binary* target 则会依赖于此 target。(可选)
* binary target：创建所有二进制包(是 binary-arch 和 binary-indep 的合并)。(必须)
* binary-arch target：在父目录中创建平台依赖(Architecture: any)的二进制包。(必须)
* binary-indep target：在父目录中创建平台独立(Architecture: all)的二进制包。(必须)
* get-orig-source target：从上游站点获得最新的原始源代码包。(可选)


rules中target的执行是这样的：
```
debian/rules target
```
就执行了rules中的target项对应的操作。  



