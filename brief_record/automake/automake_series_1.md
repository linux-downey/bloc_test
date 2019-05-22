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
* autoscan ： 扫描源代码，搜寻一些普遍的可移植性问题，,这个程序需要指定源代码树的根目录，如果不指定参数，就以当前目录为源代码根目录进行处理，并初步生成一个**configure.scan**文件,这个**configure.scan**是**configure.ac**的基础版本。  
* autoconf : 解析configure.ac，负责将用户配置的configure.ac文件转化成configure文件。
* automake ： 负责将用户编写的makefile.am生成Makefile.in文件，同时configure.ac文件也会影响makefile的生成。  
* aclocal ： 根据已经安装的宏以及用户定义的宏，将其定义到aclocal.m4中，这是一个perl脚本，自动生成aclocal.m4文件。  
* autoheader ：autoheader程序可以创建C形式的'＃define'语句的模板文件以供configure使用。它在配置源中搜索AC_CONFIG_HEADERS的第一次调用，以确定模板的名称，并将其作为输入文件，输出到config.h.in文件中。  

**configure文件生成之后，执行configure文件，它根据Makefile.in文件生成平台对应的Makefile。**   



## configure.ac文件

首先，我们有必要知道这个文件的作用，**configure.ac**将被autoconf(生成configure)和automake(生成makefile.in)工具读取并生成相应的文件.  

然后，我们需要来看看一个标准的configure.ac是怎样的布局：

    AC_INIT(package, version, bug-report-address)
    information on the package
    checks for programs
    checks for libraries
    checks for header files
    checks for types
    checks for structures
    checks for compiler characteristics
    checks for library functions
    checks for system services
    AC_CONFIG_FILES([file...])
    AC_OUTPUT
第一部分是package相关的，定义package的名称、版本以及各种信息。  
第二部分主要是检查，检查目标系统上的各种依赖软件、服务等等
第三部分就是设置生成的Makefile的各种配置文件，以及指定生成哪些makefile。  

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

**configure.ac**中包含一系列的宏定义，其值将会作为configure脚本的宏。  

以AC_为前缀的是Autoconf的宏，而以AM_为前缀的是Automake的宏。  

接下来简要分析configure.ac中各个宏的含义：
* AC_PREREQ ： 表示目标机器需要的Autoconf版本。  
* AC_INIT ：表示对Autoconf的初始化，包括当前源码包的包名，当前源码包的版本以及提交bug的地址，如果是个人源码包，可以留下自己的邮箱。
* AM_INIT_AUTOMAKE ：表示对Automake的初始化，它的语法可以是这样：

    AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_INIT_AUTOMAKE括号中的值通常是一系列的编译参数，比如上述的 -Wall -Werror foreign都是gcc的编译参数。   
* AC_PROG_CC ：这个宏提示configure脚本检查目标机器上的C编译器，并且定义"CC"这个变量，其值为gcc编译器。  
* AC_PROG_* : AC_PROG_XXX，XXX表示软件或者其他依赖程序，AC_PROG_*这一系列的指令表示检查目标平台上是否存在对应的软件.  
* AC_CHECK_* : 同样是检查目标平台上的资源,可以包括文件、库、服务等等，对于这一系列的资源检测，可以参考[官方文档](https://www.gnu.org/software/autoconf/manual/autoconf-2.61/html_node/Existing-Tests.html#Existing-Tests)  

* AC_CONFIG_HEADERS([config.h]) ： 这个指令提示configure 脚本生成一个config.h文件，config文件中包含了所有在**configure.ac**中以"#define"定义的宏，比如：

    #define PACKAGE_BUGREPORT "linux_downey@sina.com"
* AC_CONFIG_FILES : 这个宏的变量是一个列表，通常表示需要生成的makefile，例如：

    AC_CONFIG_FILES([
        Makefile
        src/Makefile
        ])
这个宏中列出的所有相关目录会导致对应目录下的*.in被转化为configure文件，同时Makefile.am被转化为Makefile。  
**需要注意的是，只要你在工程中添加一个新的目录，如果你期望新目录中的也生成相应的makefile，你就得在这个宏中添加相应目录的Makefile(如src/下就添加一个成员：src/Makefile)以指导构建工具生成Makefile，并且同时需要提供一个Makefile.am文件。**  

* AC_OUTPUT ： 这个宏表示当前文件的结束，这个宏必须存在。  


### Makefile.am文件
Makefile.am文件由用户编写，automake程序会将其转化成Makefile.in文件，在configure的执行阶段会将Makefile.in转化为Makefiele。  

Makefile.am的语法和普通的Makefile一直，不同的是Makefile.am支持更多的内置变量，需要遵循特定的命名规则。  

在上一章节中提到的Makefile.am和src/makefile.am文件是这样的：

    src/Makefile.am：

        bin_PROGRAMS = hello
        hello_SOURCES = main.c 
    Makefile.am:  

        SUBDIRS = src
        dist_doc_DATA = README

在Makefile.am中，我们更多的是关注其中的一些内建变量，可以直接很方便地使用这些变量的特性：  
* 以_PROGRAMS结尾的变量，指示makefile需要编译生成的目标。bin_PROGRAMS则表示最后再make install的时候目标会被安装到\$(bindir)中，\$(bindir)的目录为/usr/local/bin。除了安装到\$(bindir)中,还支持安装到sbindir，libexecdir、pkglibexecdir中。
    例如，将hello安装到/usr/local/sbin中就可以使用下列指令：

        sbin_PROGRAMS = hello

* \$(TARGET)_SOURCES : 表示生成目标依赖的源文件，.h文件也需要添加，目标也可以是静态库和动态库.上述示例中hello依赖main.c，所以是这样的：
    hello_SOURCES = main.c 

* 编译通常涉及到编译选项，同样的，我们可以使用_CFLAGS后缀来指定编译选项，例如添加一个-g选项：

    hello_CFLAGS = -g

* 以_LIBRARIES结尾的表示目标库文件，lib_LIBRARIES表示编译出的库文件将被安装到\$(libdir)中，\$(libdir)的目录为/usr/local/lib/. 
    如果我们需要编译一个静态库文件，可以这样写：

        lib_LIBRARIES = libhello.a
        libhello_a_SOURCES = …
    如果是动态库，我们可以这样写：
        lib_LIBRARIES = libhello.la
        libhello_la_SOURCES = …
    同样的，lib_LIBRARIES的前缀lib_表示安装目录。  
  
* 既然涉及到库文件，那么就肯定要涉及到库的链接问题，了解Makefile的朋友肯定知道，在链接时需要指定链接的目标库才能正常完成链接，所以在这里也需要用户来指定，指定链接库使用LDADD指令，比如：

    LDADD = src/libhello.a
    就是在编译的链接过程将libhello.a进行链接。  


* 事实上，某些静态库只在编译链接时用到，并不需要安装到系统目录中，这时候我们就可以使用:noinst_LIBRARIES。noinst_前缀同样适用于目标文件和动态库，比如：
    noinst_PROGRAMS = hello  
    在安装时就不会将hello安装到系统目录中。  

* 同样的，库的编译会涉及到编译选项问题，可以使用**_LDFLAGS**后缀来指定编译选项，使用方法同_CFLAGS。  

* SUBDIRS : SUBDIRS是一个特殊的变量，这个变量内容是一个列表，列出在处理当前目录下的Makefile之前需要先递归处理的目录，所以在上述示例中的SUBDIRS = src指令的意思是：在处理当前目录下的Makefile之前先需要去处理src/目录下的Makefile。  

* dist_doc_DATA ：表示目标文档将会被安装到\$(docdir)中相对应目录,\$(docdir)默认为 : /usr/local/share/doc/。带有_DATA后缀的目标并不会在make install时被自动安装，所以需要添加dist_前缀以进行安装，

事实上，Makefile.am支持大部分makefile的语法，详情可以参考[官方文档](https://www.gnu.org/software/automake/manual/automake.html#A-Program)   


autotools中两个主要的配置文件的常用选项介绍就到此为止了，在下一章节，我们将以一个稍微复杂的实例来演示autotools的用法。  
毕竟，hello_world工程只是入门，我们需要的是能够使用。  



