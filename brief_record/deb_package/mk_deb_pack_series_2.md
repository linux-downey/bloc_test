# linux下deb包的制作(2) -- 源码包和自动编译构建deb包
在上一章节中，我们讨论了一个deb包的制造，演示了从0开始创建一个hello world的二进制debian包。  

在这一章中，我们来使用官方提供的自动化构建工具来创建一个由源码生成的deb包，源码包最大的好处就是它非常不错的跨平台性能。  

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



在以上步骤完成后，debmake工具仅仅是为我们生成标准的文件列表，具体的control文件内容还是需要我们来编辑。  


