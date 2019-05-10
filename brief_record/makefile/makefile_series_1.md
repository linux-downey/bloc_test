# 深入解析Makefile系列(1) -- 进一步认识makefile
## 前情回顾
在上一篇文章中，我们以一个非常简单的makefile示例，介绍了一个makefile的执行过程以及核心原理，并留下了一道题：

    定义一个不带依赖文件列表的目标，目标的命令部分生成该目标，假设目标为名为test，它的形式是这样的：
    ...
        test:
            touch test
    ...
    调用make test 
    会发生什么？重复调用又会发生什么？为什么？
    如果将test显式定义为伪目标结果会不会一样？

如果理解了上一章节所介绍的内容，这道题是非常简单的。我们来简单分析一下：
* 当第一次调用**make test**时，make工具将会检查依赖文件列表，没有发现文件依赖列表意味着没有依赖文件的更新，接着检查是否存在目标文件，发现没有目标文件，所以该目标需要重新编译生成，所以执行**touch test**来生成目标文件。  
* 当第二次调用make test时，同样检查到没有依赖文件的更新，同时该目标文件已经生成，所以并不会执行命令。带来的问题是，如果我想更新test目标，除非删除test。  
* 如果将test显式定义为伪目标，由于makefile对于伪目标的固定规则，每次重复调用**make test**都将执行命令**touch test**。


## 进一步认识makefile

















