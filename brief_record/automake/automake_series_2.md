# linux autotools工具使用详解(2) - 完整示例
在前面的章节中，我们以一个简单的hello world讨论了autotools的基本使用。  

但是在实际的开发环境中，情况一定会更复杂，在这一节中，我们来看一个更复杂更全面的示例，其中包括：可执行文件、库的生成、链接以及安装、编译选项等等，基本满足了中小型工程的应用场景。  


## 示例程序
所有的源文件以及目录树是这样的：

    .
    ├── AUTHORS
    ├── ChangeLog
    ├── configure.ac
    ├── COPYING
    ├── Makefile.am
    ├── NEWS
    ├── README
    └── src
        ├── main.c
        ├── Makefile.am
        ├── share_lib
        │   ├── Makefile.am
        │   ├── share_lib.c
        │   └── share_lib.h
        └── static_lib
            ├── Makefile.am
            ├── static_lib.c
            └── static_lib.h
AUTHORS、ChangeLog、COPYING、NEWS、README属于源码包的各类描述文件，这些文件的作用基本上通过名称就可以明白，这里就不过多描述了。  
***  
其中，源代码的目录是这样的：

    .
    └── src
        ├── main.c
        ├── share_lib
        │   ├── share_lib.c
        │   └── share_lib.h
        └── static_lib
            ├── static_lib.c
            └── static_lib.h
顶层目录下不存在源代码，所有源代码都放置在src/目录中，
main.c为主函数文件  
share_lib/目录下的文件将被编译为动态库，在执行make clean时将被安装到系统目录
static_lib/目录下的文件将被编译为静态库，只是在编译时使用，并不安装到系统目录中
***  

按照惯例，主要的关注点在于configure.ac和Makefile.am文件。 
### configure.ac  
configure.ac文件的内容是这样的：

    1 AC_PREREQ([2.69])
    2 AC_INIT([Downey-package],[1.0.0],[linux_downey@sina.com])
    3 AM_INIT_AUTOMAKE(-Wall)
    4 
    5 AC_DEFINE([CUST_VALUE],[123],[Customize value])
    6 
    7 LT_INIT
    8 AC_PROG_CC
    9 AC_PROG_RANLIB
    10 AM_PROG_AR
    11 AC_PROG_INSTALL
    12 AC_PROG_MKDIR_P
    13 AC_CHECK_FILE(/usr/bin/install,echo find file,echo can not find file)
    14 
    15 AC_CONFIG_HEADERS([config.h])
    16 AC_CONFIG_SRCDIR([src/])
    17 AC_CONFIG_FILES([
    18         Makefile
    19         src/Makefile
    20         src/share_lib/Makefile
    21         src/static_lib/Makefile
    22 ])

对于其中大部分的参数在前两节中都有提及，这里不过多赘述，这里我们需要关注configure.ac中以下几个宏参数：
* 第五行，表示自定义一个宏，语法为
    ```
    AC_DEFINE([variable],[value],[describe..])
    ```
    第一个参数为宏名，第二个函数为宏的值，第三个参数为对于这个宏的说明。  
    这个宏最终的效果是：在AC_CONFIG_HEADERS宏指定的文件中生成一个C/C++的宏，在这个示例中是config.h，这个头文件可以被我们源代码中任何源文件包含并使用，查看config.h的内容是这样的：  
    ```
    /* Customize value */
    #define CUST_VALUE 123
    /* Define to the version of this package. */
    #define PACKAGE_VERSION "1.0.0"
    ...
    ```
* 第七行，LT_INIT表示在编译动态库时使用libtool工具进行编译，libtool工具来处理动态库相比直接编译.so文件具有更好的跨平台性能。  
* 第九、十行：AC_PROG_RANLIB，AM_PROG_AR，在需要编译库函数时，需要ranlib(linux下的程序)的支持。AM_PROG_AR是检查主机上的ar(一个打包程序)是否存在。    
* 第十一、十二行：AC_PROG_INSTALL、AC_PROG_MKDIR_P，分别检查/usr/bin/install和mkdir -p指令是否支持，前者是安装程序，后者是创建目录的程序。这一部分一般默认都支持，不写也可以。  
* 第十三行 ： 这一行纯粹是博主为了演示而添加，AC_CHECK_FILE的语法是这样的：
    ```
    AC_CHECK_FILE([file]，[action if exist],[action if does not exist])
    ```
    即查看主机上某个文件是否存在，如果存在则执行第二个参数中指令，否则执行第三个参数中的指令，指令由shell解析。  
* 第十六行：AC_CONFIG_SRCDIR,表示检查目标源文件或源文件目录是否存在。  
* 第十七行 ： 指定需要生成Makefile的目录，在前面章节中有提到。  

### Makefile.am
在本示例中一共需要四个Makefile：top、src/、src/static_lib、src/share_lib/，依次看看这些Makefile的内容：  
Makefile.am:

    1 SUBDIRS = src src/share_lib/ src/static_lib/
    2 dist_doc_DATA = README
由于顶层Makefile中并不存在源代码，所以只需要指定需要递归执行的子目录Makefile即可。 
***  
src/Makefile.am:

    1 SUBDIRS = share_lib/ static_lib/
    2 bin_PROGRAMS = helloDowney
    3 helloDowney_CFLAGS = -g -Istatic_lib -Ishare_lib
    4 helloDowney_SOURCES = main.c
    5 helloDowney_LDADD = static_lib/libstatic_hello.a  share_lib/libhello.la
同样的，因为src/目录下存在子目录，所以同样先要处理子目录中的Makefile。  
当前目录下存在main.c文件，所以可执行文件在当前目录下进行编译，第二、三、四行指定可执行文件的名称，编译参数以及依赖的源文件。  
第五行指定了编译当前可执行文件需要链接的库文件，这里指定库文件可以直接指定库文件所在目录，而不需要像Makefile中那样使用-L、-l参数来指定目录以及文件名。  
*** 
src/static_lib/Makefile.am:

    1 noinst_LIBRARIES = libstatic_hello.a
    2 libstatic_hello_a_SOURCES = static_lib.c static_lib.h
由于静态库直接编译进可执行文件中，所以不需要安装到系统目录中，我们可以使用noinst_前缀进行修饰，表示不安装。 

***
src/share_lib/Makefile.am:  

    1 lib_LTLIBRARIES = libhello.la
    2 libhello_la_SOURCES = share_lib.c share_lib.h
    3 libhello_la_CFLAGS = -shared -fPIC
在编译动态库的时候并非使用通用的.so文件，而是使用libtool工具来对动态库进行处理，由于动态库的特殊性，直接编译生成.so的做法并没有很好的可移植性，libtool是一个通用库支持程序，将使用动态库的复杂性隐藏在统一、可移植的接口中，实现动态库的高可移植性。  
所以这里使用的是libtool的语法，库的后缀为.la而不是.so，同时，编译动态库通常需要使用-shared -fPIC选项。  
第一行中的lib_LTLIBRARIES则表示在安装时将被会安装到/usr/local/lib中。  


## 构建源码包
就像一章中提到的一样，只要准备好了相应的源文件，构建整个软件包就只需要一条指令：

    autoreconf --install

**需要注意的是，某些系统上没有安装libtool，需要使用以下指令进行安装：**

    sudo apt-get install libtool


## 源码包的使用
同样的，我们可以使用以下指令进行编译安装：
* ./configure
* make
* sudo make install

**同时有一点需要注意，默认情况下，在安装时动态库将会被copy到/usr/local/lib目录中。**    
**在程序运行的时候，会在系统目录中寻找动态库并尝试链接，但是/usr/local/lib并非默认的系统库目录，所以会出现以下情况：**  

    helloDowney: error while loading shared libraries: libhello.so.0: cannot open shared object file: No such file or directory
**表示找不到动态库**。  

对于这个问题，有两种解决方案：
* 将/usr/local/lib目录添加到系统库目录中，操作方式是这样的：
```
sudo sh -c "echo /usr/local/lib >> /etc/ld.so.conf"
sudo ldconfig
```
先将/usr/local/lib添加到系统配置文件中，然后再调用ldconfig，其作用相当于刷新配置文件，重新载入。  

* 将共享库的安装路径修改为/usr/bin或者其他系统默认的库路径。  
具体做法为：将src/share_lib/Makefile.am修改为以下内容
```
1 override prefix = /usr
2 
3 lib_LTLIBRARIES = libhello.la
4 libhello_la_SOURCES = share_lib.c share_lib.h
5 libhello_la_CFLAGS = -shared -fPIC
```
唯一修改的内容为第一行：
```
override prefix = /usr
```

在Makefile.am语法中，${prefix}变量的默认值是/usr/local，在安装时默认会安装到这个目录相对应目录下，所以我们只需要在Makefile.am中修改这个变量，即可实现安装目录的修改。  

上述示例是将/usr/local修改为/usr目录。  

下文中列出了常用的内置变量对应的安装目录：
```
prefix --- /usr/local
exec_prefix --- ${prefix}
bindir --- ${exec_prefix}/bin
libdir --- ${exec_prefix}/lib
…
includedir --- ${prefix}/include
datarootdir --- ${prefix}/share
datadir	--- ${datarootdir}
mandir --- ${datarootdir}/man
infodir	--- ${datarootdir}/info
docdir	--- ${datarootdir}/doc/${PACKAGE}
```
***  

除了修改Makefile.am文件以外，还有另一种方法，就是通过命令行的方式覆盖默认的变量值：

    ./config --prefix /usr
但是这种改变将作用于所有的Makefile。  


### 示例源码
[点击获取github源码地址](https://github.com/linux-downey/automake_example)












