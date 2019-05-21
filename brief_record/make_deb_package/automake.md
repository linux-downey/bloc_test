# linux autotools工具自动化打包源码
通常，在编写编译小型程序的时候，仅仅在命令行进行编译或者是自己写个对应的makefile使用make工具进行编译。  

但是，这种做法在面临中大型工程的时候就有些捉襟见肘了。对于大型工程而言，编写makefile的工作量将非常大，而且不能保证makefile在兼容性、扩展性等等。  

不过linux下的一切难题都难不倒分布在各地的linux维护者们，各种平台、针对各种语言的开源构建工具相继出现。其中，在C语言的构件上值得称道的有cmake，其拥有语法简洁易用的特性。  

但是我们今天要讨论的并不是cmake这一款自动构建工具，而是另一个血统更为纯正的构建工具：GNU Autotools。没错，看到GNU开头的三个大字，相比你就知道了这是GNU官方组织推出的构建工具，这是一款绝对成熟的构建工具，而且几乎不用担心平台兼容问题。  

## 为什么选择autotools
autotoos工具包的目标其实是根据现有源码自动化地生成makefile，以便用户可以方便地进行编译和安装。  

通常，我们会使用autotools生成的配置文件与源码一起打包，做成一个跨平台的源码包供用户进行安装。  

使用autotools更多的是因为它的官方背景，相比于cmake，它的操作相对较为复杂，但是复杂的同时也意味着灵活，同时所有的发行版都支持autotoos。  

所以在制作需要发布或者代表官方的源码包时，我们有必要使用更为标准的autotools。  

## 自动构建工具的作用
在上文中提到的自动构建工具到底是什么。有些朋友对于这个概念并不是太熟悉，我们就来看看这个构建工具到底做了什么：
* 可配置地提取工程中的源码，为源码建立编译规则
* 生成配置文件，在一台新机器上进行编译时可以根据目标机器的架构、平台生成对应的makefile，这也是实现平台无关性的关键。  
* 生成的makefile支持make install、clean等常用编译选项，覆盖大多数基本需求。  

## autools介绍
autotools实际上包含一系列的工具，主要的包括有autoscan、autoconf、autolocal等工具，在制作源码包的过程中这些工具会被分别调用以实现相应的功能。  

现在我们来梳理一下使用autotools自动构建工具生成标准makefile的流程。  

### autotools使用流程
#### 1、准备源代码
#### 2、使用autoscan命令
使用指令：**autoscan**，这条指令会搜索源代码，生成两个文件：**autoscan.log**，**configure.scan**，我们需要关注的是**configure.scan**文件，这个文件是主要的配置文件。  
将**configure.scan**改名为**configure.ac**：

    mv configure.scan configure.ac
然后编辑修改configure.ac中的配置以适应目前的工程。  
默认情况下，生成的**autoscan**文件是这样的：

    1 #                                               -*- Autoconf -*-
    2 # Process this file with autoconf to produce a configure script.
    3 
    4 AC_PREREQ([2.69])
    5 AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
    6 AC_CONFIG_SRCDIR([src/hello_lib.h])
    7 AC_CONFIG_HEADERS([config.h])
    8 
    9 # Checks for programs.
    10 AC_PROG_CC
    11 
    12 # Checks for libraries.
    13 
    14 # Checks for header files.
    15 
    16 # Checks for typedefs, structures, and compiler characteristics.
    17 
    18 # Checks for library functions.
    19 
    20 AC_OUTPUT

首先，我们有必要知道这个文件的作用，**configure.ac**(由**configure.scan**改名而来)将被autoconf(生成configure)和automake(生成makefile.in)工具读取并生成相应的文件.  

**configure.ac**中包含一系列的宏定义，其值将会作为configure脚本的宏。  

以AC_为前缀的是Autoconf的宏，而以AM_为前缀的是Automake的宏。  

* AC_PREREQ ： 表示目标机器需要的Autoconf版本。  
* AC_INIT ：表示对Autoconf的初始化，包括当前源码包的包名，当前源码包的版本以及提交bug的地址，如果是个人源码包，可以留下自己的邮箱。
* AM_INIT_AUTOMAKE ：表示对Automake的初始化，它的语法可以是这样：

    AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_INIT_AUTOMAKE括号中的值通常是一系列的编译参数，比如上述的 -Wall -Werror foreign都是gcc的编译参数。(尽管这个宏在上述**configure.ac**文件中没有出现，但是它值得被提及)    
* AC_PROG_CC ：这个宏提示configure脚本检查目标机器上的C编译器，并且定义"CC"这个变量，其值为gcc编译器。  
* AC_CONFIG_HEADERS([config.h]) ： 这个指令提示configure 脚本生成一个config.h文件，config文件中包含了所有在**configure.ac**中以"#define"定义的宏，比如：

    #define PACKAGE_BUGREPORT "linux_downey@sina.com"
* AC_CONFIG_FILES : 这个宏的变量是一个列表，通常表示需要生成的makefile，例如：

    AC_CONFIG_FILES([
        Makefile
        src/Makefile
        ])
这个宏中列出的所有相关目录会导致对应目录下的*.in被转化为configure文件，同时Makefile.am被转化为Makefile。  
**需要注意的是，只要你在工程中添加一个新的目录，如果你期望新目录中的也生成相应的makefile，你就得在这个宏中添加相应目录的Makefile以指导构建工具生成Makefile，并且同时需要提供一个Makefile.am文件。**  

* AC_OUTPUT ： 这个宏表示当前文件的结束，这个宏必须存在。  

#### 3、手动创建依赖文件
在使用autotools时，有些文件需要










新建Makefile.am文件，并编辑Makefile.am文件

使用autoconf来生成configure文件

执行configure生成Makefile。  

添加子目录:
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_SUBDIRS($DIR)

编译报错：
configure.ac:8: error: required file 'config.h.in' not found
    执行autoheader
configure.ac: error: no proper invocation of AM_INIT_AUTOMAKE was found.
    在configure.ac中添加AM_INIT_AUTOMAKE项

touch NEWS README AUTHORS ChangeLog
创建几个必须的文件，没有这些文件



实验文件：
hello.c
src/hello_lib.c
src/hello_lib.h



指令：
* autoscan(生成configure.scan)
* 手动创建NEWS README AUTHORS ChangeLog文件
* aclocal
* autoheader
* autoconf(生成configure文件)
* automake -a(automake --add-missing)自动创建一些目前没有的目录。
* ./configure
* make 
* make install

个人修改的文件：
将configure.scan改名为configure.ac

    1#                                               -*- Autoconf -*-
    2 # Process this file with autoconf to produce a configure script.
    3 
    4 AC_PREREQ([2.69])
    5 AC_INIT([hello], [1.0.0], [linux_downey@sina.com])
    6 #AM_INIT_AUTOMAKE([hello],[1.0.0])
    7 AC_PROG_RANLIB
    8 AM_INIT_AUTOMAKE([subdir-objects])
    9 AC_CONFIG_SUBDIRS([src])
    10 AC_CONFIG_SRCDIR([hello.c])
    11 AC_CONFIG_HEADERS([config.h])
    12 
    13 # Checks for programs.
    14 AC_PROG_CC
    15 
    16 # Checks for libraries.
    17 
    18 # Checks for header files.
    19 
    20 # Checks for typedefs, structures, and compiler characteristics.
    21 
    22 # Checks for library functions.
    23 
    24 AC_CONFIG_FILES(Makefile src/Makefile)
    25 AC_OUTPUT()

在当前目录下，创建一个Makefile.am
同时，也要在src目录下创建一个Makefile.am

主目录下的Makefile内容：

    1 bin_PROGRAMS = hello
    2 SUBDIRS = src/
    3 #helloincludedir = src/
    4 #helloinclude_HEADERS = src/hello_lib.h
    5 
    6 hello_SOURCES = hello.c src/hello_lib.h
    7 hello_LDADD = src/libhello.a

bin_PROGRAMS表示目标文件，SUBDIRS表示在处理当前目录之前需要先处理的子目录
helloincludedir和helloinclude_HEADERS表示需要安装到系统中的头文件
hello_SOURCES：表示编译hello的文件
hello_LDADD：表示在链接过程需要链接一个静态库。  

在src目录下的Makefile.am主要任务是将src/hello_lib.c编译成libhello.a静态库

    1 lib_LIBRARIES = libhello.a
    2 
    3 libhello_a_SOURCES = hello_lib.c




各级子目录都生成相应的Makefile，分离式编译。  





确认一下，头文件的问题，是否直接接在source后面。 





官方文档：https://www.gnu.org/software/automake/manual/automake.html
https://www.ibm.com/developerworks/cn/linux/l-makefile/index.html 博客参考

