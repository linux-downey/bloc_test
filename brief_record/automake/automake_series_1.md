# linux autotools工具使用详解(1) - 配置文件

## 章节回顾
在上一章节中，我们提到了怎么使用**autoreconf**来全自动地生成源码包，生成的源码包可以通过
* ./configure
* make
* sudo make install
这三个步骤将软件安装到系统中。  
在上一章节中主要用到了5个文件分别是：  

    src/main.c src/Makefile.am README Makefile.am configure.ac 
其中，main.c为源文件，README为源码包的说明文件，在这一章节中，我们主要介绍配置文件的语法，这是整个autotools的核心部分。  

## autotools中各软件的作用
在上一章节中，使用**autoreconf**命令就可以自动地由Makefile.am生成Makefile，由configure.ac生成configure文件。  

同时提到，**autoreconf**命令其实是以一定顺序调用了多个**autotools**中的程序，才实现了相应的功能，接下来，我们就看看**autotools**中主要的一些程序：  
* autoscan ： 扫描源代码，搜寻一些普遍的可移植性问题，并初步生成一个**configure.scan**文件,这个**configure.scan**是**configure.ac**的基础版本。  
* autoconf : 负责将用户配置的configure.ac文件转化成configure文件。
* automake ： 负责将用户编写的makefile.am生成Makefile.in文件。
* aclocal ： 根据已经安装的宏以及用户定义的宏，将其定义到aclocal.m4中，这是一个perl脚本，自动生成aclocal.m4文件。  
* 
* configure文件生成之后，执行configure文件，它根据Makefile.in文件生成平台对应的Makefile。


## configure.ac文件
在上一章节中，我们的configure.ac配置文件是这样的：

    AC_INIT([amhello], [1.0], [bug-automake@gnu.org])
    AM_INIT_AUTOMAKE([-Wall -Werror foreign])
    AC_PROG_CC
    AC_CONFIG_HEADERS([config.h])
    AC_CONFIG_FILES([
    Makefile
    src/Makefile
    ])
    AC_OUTPUT

首先，我们有必要知道这个文件的作用，**configure.ac**将被autoconf(生成configure)和automake(生成makefile.in)工具读取并生成相应的文件.  

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


