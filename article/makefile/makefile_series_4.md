# 深入解析Makefile系列(4) -- 多目录处理

在之前的章节中，我们都是在讨论在单目录下简单的工程编译，当涉及到较为复杂的工程时，通常复杂的逻辑会以模块的形式进行分离编译，这样有利于大工程的扩展以及维护，这通常要涉及到多个目录下的makefile操作，以及各种复杂的逻辑处理。  

在这一章节，我们就来讨论复杂情况下的makefile编写规则，主要包含目录的处理以及条件语句。  

## 多目录下的makefile
相信大多数从事linux环境下编程的程序员对linux内核的编译并不陌生，即使不用亲自动手来写编译linux内核的makefile，也会有或多或少的接触。  

linux编译内核的makefile名为kbuild编译系统，整体分布式的架构，主目录的makefile负责主要的处理，并递归地进入每个子目录中执行对应的makefile，最后进行链接生成相应的镜像，当然说来简单，事实上是非常复杂的，有机会我们再详细讨论。  

今天我们就来讨论多层目录下的makefile是怎么实现工程编译的,在讨论之前我们需要了解一些关于多文件编译的知识点。    

### include
在makefile的语法中，支持像C/C++一样直接包含其他文件，include语法：

    include filename
这个include指令告诉make，挂起读取当前makefile的行为，先进到其他文件中读取include后指定的一个或者多个Makefile，然后再恢复处理当前的makefile。  
include指令同时支持模式规则，通配符，和变量的使用，例如：

    bar = bar.mk
    include *.c $(bar) 
被包含的文件不需要采用makefile默认的名称：比如GNUmakefile，makefile或Makefile。  
通常情况下，使用include的场景是：
多个目录下的文件编译由各个makefile分布式处理，需要使用同一组变量或者模式规则。  
或者是， 

举一个简单的示例，同目录下存在四个文件：

    Makefile foo.c bar.c inc.mk

inc.mk为被包含的文件，内容为：

    SRC += bar.c
Makefile为主makefile文件，内容为：

    SRC = foo.c
    include inc.mk
定义一个SRC变量，并包含inc.mk文件中的操作，最后$(SRC)的结果为：foo.c bar.c.   

表明文件的包含关系中共享变量，我们可以直接简单地理解为将被包含文件中的数据添加到主文件中。  


### 指定搜索目录
很多新接触gcc和makefile的朋友会有一个疑惑：为什么在依赖文件中需要指定对应的.h文件，而在编译的时候就不需要将.h文件添加进去。  
比如在目录中存在这么一些文件：

    main.c foo.c foo.h
makefile的实现是这样的()：

    main: main.c foo.c foo.h
        cc main.c foo.c -o $@
可以看到，在依赖文件中存在foo.h，在命令部分就不需要添加foo.h，难道foo.h不参与编译吗？  
答案是：foo.h当然是参与编译的，只是它并不需要显示地指定，.h文件通常是被当做头文件包含在foo.c中,gcc在编译的预处理阶段会将"#include"指定的头文件添加到当前.c目录中，所以这个过程并不需要显式地指定。  

在执行多目录下的工程编译时，make默认在当前目录下搜索文件，如果需要指导make去指定搜索其它目录，我们需要使用 -I 或者 --include-dir 参数来指定。  

事实上，执行编译的过程是makefile中的命令部分，这一部分是由shell进行解析的，-I 其实是shell下gcc的编译指令。通常，用法是这样的：

    -I. -I../dir/

除了使用gcc的参数"-I"，我们还可以使用makefile中自带的变量"VPATH"，通过环境变量"VPATH"指定的目录会被make添加到目录搜索中。  

在VPATH中添加的目录，即使是文件处于其他目录，我们也可以像操作当前目录一样操作其他目录的文件，例如：

    VPATH += src
    all:foo.c
        cc $^ -o $@
等效于：

    all:src/foo.c
        cc $^ -o $@

但是写成下面这样是不行的：

    VPATH += src
    all:
        cc foo.c -o $@
**这是因为：VPATH是makefile中的语法规则，而命令部分是由shell解析，所以shell并不会解析VPATH，而是将其当成编译当前目录下的foo.c。**


### 目录搜索的规则
在makefile语法中，目录搜索的流程是这样的：
* 如果一个目标文件不存在于指定的目录中，开始目录搜索。  
* 如果目录搜索成功，则保留路径，并暂时将该文件存储为目标
* 使用上述相同的方式检查所有的先决条件。  
* 执行完所有依赖文件的检查后，就可以确定目标是否需要重新编译。当需要重新编译时，将会在本地进行重新编译，而不是选择在搜索到目录的地方进行编译。  

### 切换目录并编译
在分布式编译的情况下，也就是一个工程多个目录对应多个makefile，我们通常需要切换到其他目录下进行编译，这时候我们就需要使用到make的"-C"选项：

    make -C dir
需要注意的是，"-C"选项只支持大写，不支持小写。  

make将进入对应目录，并搜索目标目录下的makefile并执行，执行完目标目录下的makefile，make将返回到调用断点处继续执行makefile。  

利用这个特性，对于大型的工程，我们完全可以由顶层makefile开始，递归地遍历整颗目录树，完成所有目录下的编译。  


### 多目录makefile共享环境变量
与shell类似，makefile同样支持环境变量，环境变量分为两种：
* 对应运行程序
* 对应程序运行参数

下面是常用环境变量的列表(针对C/C++编译，其他语言不列出)：  

对应运行程序的环境变量：  
* AR         打包程序，默认值为ar，对目标文件进行打包，封装静态库
* AS         汇编程序，默认值为as，将汇编指令编译成机器指令
* CC         c编译器，默认值为cc，通常情况下，cc是一个指向gcc的链接，负责将c程序编译成汇编程序。  
* CXX        c++编译器，默认值为g++
* CPP        预处理器，默认值为 "$(CC) -E"，注意这里的CPP不是C++，而是预处理器。 
* RM         删除文件，默认值为 "fm -f"，-f表示强制删除

对应程序运行参数的环境变量：
* ARFLAGS        指定$(AR)运行时的参数，默认值为"rv"
* ASFLAGS        指定$(AS)运行时的参数，无默认值
* CFLAGS         指定$(CC)运行时的参数，无默认值
* CXXFLAGS       指定$(CXX)运行时的参数，无默认值
* CPPFLAGS       指定$(CPP)运行时的参数，无默认值，注意这里的CPP不是C++，而是预处理器
* LDFLAGS        指定ld链接器运行时的参数，无默认值
* LDLIBS         指定ld链接器运行时的链接库参数，无默认值。  

这些默认的环境变量将在执行make时传递给makefile。  

同样的，也可以使用export指令将特定的变量添加到环境变量中，但是，通过export指定添加的环境变量只作用于当前makefile以及递归调用的子makefile中，对于同目录下或者非递归调用的其他目录下的makefile是不起作用的。  

为了更好地理解，我们来看下一个例子：
有两个makefile，一个位于当前目录，一个位于子目录dir/下

    makefile dir/makefile

当前目录的makefile是这样的：

    AR += XYZ
    export AR
    all:
        make -C dir/
        @echo main makefile
dir/makefile是这样的：

    all:
        @echo $(AR)

执行make输出结果：

    make -C dir/
    make[1]: Entering directory '/home/downey/mkfl_test/subdir/dir'
    ar XYZ
    make[1]: Leaving directory '/home/downey/mkfl_test/subdir/dir'
    main makefile

执行过程为：
* AR赋值，并export AR
* 进入子目录dir
* 打印子目录中的环境变量AR，结果为 ar XYZ，说明共享了上级目录makefile的环境变量
* 回到上级目录执行makefile

使用export指令，可以实现多个目录下的makefile共享同一组变量设置，对于多目录下的编译提供了很大的遍历。  

同时需要注意的是，不同makefile中，即使有调用关系，变量是不共享的。**而且,即使在makefile中修改了环境变量，不使用export更新环境变量，在其调用的子makefile下不会共享其修改，还是默认的环境变量。**  

如果想共享不同makefile中的变量，可以通过"include filename"的方式来实现。  


### 链接库的目录搜索
在makefile的目标编译规则中，依赖文件同样也可以是库文件，库文件与普通文件不一样。  

首先，库文件的命名需要遵循unix规范：以lib为前缀，以.a(静态库)或者.so(动态库)为后缀，libxxx.a或者libxxx.so，而库名为xxx。

当其作为依赖文件或者是参数命令的编译时，使用-lxxx来指定对应的库，如果库不存在于当前目录下，还需要为其指定搜索路径，使用"-L"参数指定。

一般而言，动态库会存在于系统目录(/usr/local/lib,/lib/,/usr/lib)中，并不需要指定相应的目录，而静态库存在于工程目录中，则可能需要使用-L参数来指定，我们再来看相应示例：

    all:foo.c -lmylib
        cc  $< -L. -lmylib -o main
示例中，使用-L.指定库搜索目录为当前目录，使用-l指示链接mylib库。  

***  
***    
***  

参考资料：[官方文档](https://www.gnu.org/software/make/manual/make.html)

***  
好了，关于 **深入解析Makefile系列(4) -- 多目录处理** 的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.




