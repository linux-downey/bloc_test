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
执行完成之后，会发现整个源码包目录下多了很多配置文件。执行tree查看整个目录的结构(如出错，请看下一小节**debmake纠错**)：    

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
推荐使用这种方法，最后设置的"DEBEMAIL"和"DEBFULLNAME"变量值会在control文件中使用。  

2、找到脚本出错位置，然后修改它，对比第一种方案而言，这是治本的办法，但是修改系统里面的东西总是需要非常谨慎的。首先根据提示找到 **/usr/lib/python3/dist-packages/debmake/para.py**出错未见的44行，既然这条语句有问题，那我们就将其换一种获取用户名的方法：
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
**在这些文件当中，changelog、control、copyright、postrm、postinst的应用以及修改在上一章节已经详细介绍过，这里就不再赘述，我们主要看看其他的文件(这些文件需要根据项目实际情况做修改)。**

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

#### 对rules的处理
事实上，使用默认的rules脚本是不能正常工作的，我们需要屏蔽掉其中的一个规则(默认使用所有的规则)：dh_usrlocal。  

我们可以在上述的rules文件中的17行后添加一行：
```
override_dh_usrlocal:

```
override_表示覆盖写，以空指令覆盖dh_usrlocal就可以达到目的了。  

### 使用脚本生成deb包
在所有文件修改完成之后，就可以使用以下指令来编译deb包了，在系统生成的与debian同级目录下输入：
```
debuild -us -uc
```
不出意外，就会在debian的父目录中生成一系列的文件，其中一个以.deb结尾的就是我们要的deb包了。  


## 实用扩展
事实上，完成一个hello world是非常简单的，但是如果仅仅满足于完成一个hello world工程，那我们将无法学到任何东西，因为总是完成一件事最容易的部分是最低效的学习方式。  

所以，我们需要继续深入，才能了解更多相关的知识。   

使用自动化构建工具编译deb包最主要的两个部分是：rules规则和debuild指令，我们就来看看这两个指令。  


### debuild命令
#### debuild指令的实质
事实上，debuild命令的结果是执行一个perl脚本，我们可以通过以下指令找到它：  
```
which debuild
```
一般情况下，输出结果是这样的：  
```
/usr/bin/debuild
```
打开这个可执行文件，就可以看到脚本的内容。  

#### debuild指令作用
debian指令生成deb包的所有必要文件，事实上它包含了一系列的指令：  

* 首先，它会执行**dpkg-buildpackage**,这个指令完成一系列复杂的事情，包括：
    * 设置变量，调用dpkg-source --before-build做一系列准备工作
    * 检查编译依赖和冲突。
    * 调用fakeroot debian/rules clean清除源码树
    * 调用dpkg-source -b生成源码包
    * 调用build钩子函数，调用debian/rules build-target，fakeroot debian/rules binary-target等完成源码的编译，调用一系列dh_开头的命令。  
    * 调用dpkg-genchanges生成.changes文件并配置  
    * 调用dpkg-source --after-build清除某些编译结果.  
    * 对源码包进行简单的检查，调用一系列dh_开头的命令。  
    * 如果指定需要gpg签名，则给.dsc文件签名。    
fakeroot的作用是使用虚拟的root权限来执行deb包安装、清除这些需要root权限才能执行的情况，同时将创建的文件权限、属主改为root执行的情况。   
因为在一般情况下，deb包制作者并不会使用root用户来编译deb包，这就造成了生成的文件权限和属主不符合系统要求的情况(因为deb包中的某些文件需要安装到系统中)，同时，在检查deb包的安装和清除时，如果使用真实的root权限，很可能因为各种异常而给系统带来文件垃圾。fakeroot完美地解决了这个问题。    
* 然后，执行lintian 或者 linda，主要是lintian指令，这个指令主要是对deb包进行一系列的检查工作，主要用于检查二进制和源包是否符合Debian规范以及检查其他常见的打包错误  
* 最后，根据选项执行deb包的签名工作，调用debsign，通常，正规的deb包都需要使用gpg对其进行签名，然后才能进行发布工作。但是在制作个人包的时候可以选择先不签名或者在随后使用**debsign**手动签名。   


#### debuild使用选项
在上述的编译工作中，我们直接使用指令：
```
debuild -us -uc
```
与其他指令有所区别的是，debuild指令是一个命令集合的脚本，它的执行选项同时可以包含几个指令的选项：
```
debuild [debuild options] [dpkg-buildpackage options] [--lintian-opts lintian options] [--linda-opts linda options]   
```
第一部分选项是debuild的选项，第二部分为dpkg-buildpackage的选项，第三四部分为lintian和linda的选项。  

所以，在使用debuild时，使用熟知的**debuild --help**可能并不能查看相应的选项，所以我们需要同时去查找dpkg-buildpackage或者lintian的指令。  

这里的 **-us -uc**指令是**dpkg-buildpackage**的选项，它的说明是这样的：
```
-us    Do not sign the source package.
-uc    Do not sign the .changes file.
```
为了简化deb包的生成过程，这里就不使用gpg对其进行签名了(在后面的章节将讲到gpg和deb仓库的创建)。    


### 自定义rules文件
#### 调试变量设置
在上述默认生成的rules文件中，可以将**DH_VERBOSE = 1**这一行的注释去掉，这样在执行各类dh命令时就会输出相应的信息。

#### 规则
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
就执行了rules中的target项对应的操作，在自定义的rules文件中，上述提到的带有(必须)后缀的目标是必须被加上的，当然也可以使用模式规则。  

#### 规则对应程序的调用
在rules文件中，每一个特定的目标都对应的特定功能，比如：
* debian/rules clean 运行了 dh clean，接下来实际执行的命令为：
```
dh_testdir
dh_auto_clean
dh_clean
```
* debian/rules build 运行了 dh build，其实际执行的命令为：
```
dh_testdir
dh_auto_configure
dh_auto_build
dh_auto_test
```

* fakeroot debian/rules binary 执行了 fakeroot dh binary，其实际执行的命令为：
```
dh_testroot
dh_prep
dh_installdirs
dh_auto_install
dh_install
...
...
dh_md5sums
dh_builddeb
```
* fakeroot debian/rules binary-arch 执行了 fakeroot dh binary-arch，其效果等同于 fakeroot dh binary 并附加 -a 参数于每个命令后。  
* .....


#### 规则与Makefile的关系
规则的调用当然是遵循某些规则的，它几乎是完全根据Makefile中的实现来调用的，比如：
* dh_auto_clean  通常在 Makefile 存在且有 distclean 目标时执行以下命令
```
make disclean
```
* dh_auto_configure通常在configure文件存在的时候调用以下命令：
```
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var ...
```
* dh_auto_build通常执行Makefile中的第一个目标，也就是相当于执行：
```
make
```
* ....

#### 规则的覆盖
在rules文件中，可以使用override_前缀对某个指令进行覆盖，由此可以自定义一些行为：
```
override_dh_auto_build:
        echo hello_world
```
这样，在执行dh_auto_build时，仅仅打印出hello_world而不做任何其他事。  


在一些中小型项目中，通过debmake自动生成的文件可以直接使用，但是在复杂的项目中，我们就需要对rules进行自定义才能完成相应的需求，或者对其进行针对性的优化。  


参考资料[官方文档](https://www.debian.org/doc/manuals/maint-guide/dreq.en.html#customrules)，建议参考英文文档，中文有部分翻译错误！



