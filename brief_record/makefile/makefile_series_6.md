# 深入解析Makefile系列(6) -- makefile编写实用技巧  

在前面的章节中，博主已经介绍了makefile中大部分常用的知识点，这一章节，博主打算分享一些makefile编写时的实用小技巧。  

## 确定当前目录
makefile的执行不像shell脚本的执行，可以通过"$(0)"来判断当前目录的绝对路径，makefile的执行通常是直接敲命令：make，而不需要指定makefile文件位置，那么我们应该怎么去判断当前运行的makefile处于哪个目录下呢？  

有些朋友立马就能想到，在makefile中是可以运行shell指令的，我们可以通过这样来确定当前文件位置：

    location = $(shell pwd)
或者

    location != pwd

在大多数情况下，这种做法是可以的。  

但是，如果makefile并非在本目录被执行，比如，在/home/downey目录下执行/home/downey/src目录下的makefile：

    make -f src/makefile
那么，使用shell指令"pwd"执行的结果就成了执行make的目录，即/home/downey/，而并非makefile所在的目录/home/downey/src。  

在这个时候，我们需要使用一个makefile内置变量来辅助确定makefile位置：MAKEFILE_LIST。  

MAKEFILE_LIST是一个列表，存储被make解析的makefile文件，以空格隔开。存储的顺序为调用的顺序，当当前makefile使用"include"指令包含其他文件时，MAKEFILE_LIST就会添加被包含的文件。   

所以，在没有include其他makefile文件的情况下，MAKEFILE_LIST的第一个元素就是当前makefile相对于执行make目标的文件位置，可以用下面的方法获取当前makefile路径：

    location = $(shell pwd)
    file_location = $(location)/$(firstword $(MAKEFILE_LIST))


## 调试输出

### 使用warning()函数调试变量

在第一章中我们就有强调，在makefile中，大部分规则都遵循makefile的语法。而在目标规则(目标，依赖，命令)的命令以及以shell()函数执行的部分，是由shell来处理的，遵循shell的规则。  

所以是，事实上，如果我们想实时查看makefile中某个变量的值，我们是不能在文本中使用"echo"命令输出变量值的，那么我们在调试的时候应该怎样输出变量的值的？难道只能在命令部分去输出吗？  

不！我们可以借助makefile中的warning()函数，warning()函数本身是输出警告信息，它的语法是这样的：

    $(warning text)
它可以实现在makefile中输出变量值，或者其他警告信息，而且不会像error()函数一样中断函数的执行。  

**需要注意的是，调试输出的扩展方式为默认将临时转换为简单扩展。**

例如：

    var = $(var1)
    $(warning var is $(var))
    var1 = foo.c

执行时输出结果为：

    Makefile:2: var is 
如果将makefile改为：

    var = foo.c
    $(warning var is $(var))

执行时输出为：

    Makefile:3: var is foo.c

在输出时会将命令所在行号以方便查看。  


### 命令行选项调试make执行流程
如果想快速理清一个新makefile的执行流程，我们通常可能会直接执行它，然后查看相应的log信息。   

事实上，makefile提供一个选项，它可以只输出makefile的执行信息而不真正地执行当前makefile指定的动作：

    make --just-print
这样，碰到大型的makefile程序，当我们需要阅读其执行流程的时候，完全不需要等待它执行完成。  

这对于理解一个makefile是非常有帮助的。  


### 使用--debug选项查看makefile执行流程
在makefile执行时，默认会输出命令部分信息，这些信息有助于我们判断makefile的执行情况。  

但是，有时候，仅仅输出命令部分的信息并不足以让我们对make的执行有完整的了解，以至于在调试时某些关键的细节被隐藏，非常不方便调试。  

这时，我们就可以使用makefile中另一个调试选项：

    make --debug
一看这个选项名就知道这是正统的调试选项，它输出的调试信息也是最详细的。它会输出每一步输出的详细流程，对于调试时非常方便的。  


### 多文件依赖时修改公共头文件
通常，我们在写程序时，将程序以模块为划分，但是通常会有一些共用的设置，放在一个共用的头文件中，比如common.h，又或者是main.h。  
有时候，因为修改了某个文件中的某个设置，我们需要修改这些公共的头文件，按照makefile的规则来看，因为几乎所有目标都依赖了公共头文件，我们不得不重新编译整个工程，如果是小工程还好说，如果是linux内核那样的大工程，这样是无法忍受的。  

比如下面这个示例，目录下有如下文件：

    main.c foo.c foo.h bar.c bar.h common.h 
所有的.c文件都依赖common.h文件，当由于需要修改foo.c时导致需要修改common.h时，因为所有文件都依赖common.h文件，所以所有文件都会需要重新编译一遍，但是实际上，我们仅仅只需要编译foo.c和common.h文件，而不是全部的文件。   

这时候我们可以这样做：  
* 重新编译foo.c目标，这个目标可能是foo.o，我们只需要调用命令：make foo.o  
* 然后调用指令make -t，这个-t选项将所有文件全部设置为最新状态，也就是下一次编译的时候不需要重新编译这些模块，因为make检查这个目标都是最新的。  
* 再将编译时的总目标，也就是main删除，这样编译在执行make时又会重新生成main，而此时其他所有的目标文件都为最新状态，所以这个时候就只更新了foo.c common.h main(目标文件)，而不用将所有其他没有修改的文件重新编译。  


















