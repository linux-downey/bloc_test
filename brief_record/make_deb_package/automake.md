使用autoscan，configure.scan

将configure.scan重命名为configure.ac，并编辑configure.ac

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

