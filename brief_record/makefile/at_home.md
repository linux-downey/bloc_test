## 继续深入makefile

在上一篇博客我我们讲解了一个简单的makefile的写法，demo肯定是不够的，在这一章我们再来深入了解makefile的特性。


## 生成中间文件，分布式编译
在上一章中，由于整个工程非常简单，我们将所有文件都放在一条指令中进行编译，随着编译文件的增加，我们会发现这样做明显是不对的。  

根据上一章节的编译规则中可以了解到，make在编译前会检查文件的更新，如果某个目标的依赖文件列表中有文件更新，那么重新编译这个目标，所以，如果一个目标依赖很多文件，其中只要有一个文件有修改，那么所有的这些依赖文件都要重新编译，这样是不符合用户的初衷的。  

那么，这种问题该怎么解决呢？我们看下面的例子：
    1 main:foo.o bar.o main.c common.h
    2     cc foo.o bar.o main.c common.h -o main
    3 foo.o:foo.c foo.h common.h
    4     cc -c foo.c foo.h common.h  
    5 bar.o:bar.c bar.h common.h
    6     cc -c  bar.c bar.h common.h
    7 clean:
    8     rm -rf *.o  main
上述示例中一共涉及到6个文件的编译：foo.c,foo.h,bar.c,bar.h,common.h,main.c  

根据惯例：  
foo.c依赖foo.h,common.h
bar.c依赖foo.h,common.h
main.c依赖common.h

这里的做法是先将foo.c编译成foo.o目标文件，然后将bar.c编译成bar.o目标文件，最后由foo.o,bar.o,main.c编译生成main.c，采用这种分离式的编译由什么好处呢？  
假设我们修改了main.c文件，foo.o文件和bar.o文件不用重新编译生成，因为满足两个条件：目标文件存在且依赖文件没有更新。  

我们仅仅需要做的就是重新编译main.c,然后链接foo.o和bar.o,以达到节省编译时间的目的。  


按照上一节的写法，是这样的：

    main:foo.c foo.h bar.c bar.h main.c common.h
        cc foo.c foo.h bar.c bar.h common.h main.c -o main
无论修改哪一个文件，foo.c，bar.c，main.c三个文件都要重新编译一遍。  


## 自动推导规则
makefile中，带有一些隐式规则，make的自动推导依赖文件就是其中一种。  

可以明确的是，make工具是服务于程序编译的，程序的编写自然会有一些标准可循。  

比如，在C/C++程序的编写中，我们通常将函数的实现与声明分开放，使用同名不同后缀的文件分别表示实现文件与声明文件,在C中，通常是xxx.c与xxx.h这种对应的关系。  

在makefile的规则中则利用了这种特性，由此，我们可以改写上面的示例：

    1 main:foo.o bar.o common.h
    2     cc foo.o bar.o main.c -o main
    3 foo.o:common.h foo.h
    4 bar.o:common.h bar.h
    5 clean:
    6     rm -rf *.o  main

这个makefile最终的结果与上面的makefile一致，可以发现，在编译foo.o和bar.o时，我们并不需要添加编译目标的命令，因为make会对目标进行隐式推导：make为foo.o自动寻找foo.c文件，并将foo.c编译成foo.o，这一隐式规则对于用户来说是非常方便的，只要遵循了相应的命名规则。  
需要注意的是，需要将foo.h的添加到依赖中。为什么？




隐式推导规则
变量的使用
添加注释
常用的通配符以及变量替换
几种变量的赋值方式以及延迟赋值
简单的条件处理
仅common改变
文件的搜索




























