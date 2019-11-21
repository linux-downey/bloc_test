# 深入解析Makefile系列(2) -- 常用的通配符、內建变量和模式规则
在上一章节中，我们讲解了makefile中.o文件相对于.c文件的自动推导与变量，掌握了这两项，大大方便makefile的编写维护以及扩展。  

在这一章节中，我们继续深入makefile，来讨论另一个利器：通配符。  

## makefile中的通配符
不论是在shell环境中还是在其他脚本中，通配符通常都是支持的，它提供了一种灵活的方法，以使得用户可以对任何带有共性的对象很方便地进行统一处理。   

makefile语法中的主要使用的通配符有 "\*","?"，比如：

    *.c
则表示所有文件名以.c结尾的文件。

## 各个通配符的使用

### "\*"
若是要选出通配符中出场率最高之一，**\*** 可谓是当仁不让，**\*** 表示匹配所有任何符合条件的。  
**\*.o**表示所有的.o文件   

**\*.c**表示所有的.c文件  

**\*** 表示所有的文件   

### "?"
"?" 通常在依赖文件列表中使用，匹配所有有更新的目标。  

在第一章节中我们提到，make在编译目标时，会去检查目标的依赖文件列表是是否有文件更新，$? 表示当前依赖列表中已经更新的依赖文件。我们来看以下的示例：

    1 main:foo.c bar.c
    2     @echo $?
    3     touch main
第一次运行**make**时，输出

    foo.c bar.c
    touch main
$? 的输出为foo.c,bar.c，因为这两个文件都是第一次编译。  

此时，我们对**foo.c**运行**touch**命令更新**foo.c**的时间戳，再运行**make**：
输出为：

    foo.c
    touch main
此时，$? 的值就是更新的文件foo.c。  

需要注意的是，在目标没有被生成的时候，make工具会将所有的依赖文件视为更新的文件。  

比如在上述示例中，在第一次执行**make**之后，手动删除生成的**main**文件，$? 的值将会是所有的依赖文件。  

值得注意的是，在shell中，$? 表示上一条指令的执行结果，这里需要做相应区分。  


### 通配符的转义
当我们需要输入真实的字符 \* 时，为了避免这些特殊符号被识别为通配符，需要对这些字符进行转义，比如：要操作一个真实的文件名为 \*.c 时,不能直接这样输入：

    obj = *.c
在当前目录下存在.c 文件时，将导致obj被赋值为所有后缀为.c的文件列表，应该使用反斜杠"\\"对特殊符号进行转义。应该是这样的：

    obj = \*.c
这样obj的内容就是*.c了。  

需要注意的是，在windows系统中，文件的路径分隔符是"\"而不是unix下的"/"，尽管windows同时支持windows风格和unix风格的文件分隔符，但是这种支持并不包括通配符的扩展。所以，在windows下执行makefile时需要特别注意这一点。  
***    

## 通配符在赋值时的陷阱
学习了makefile中通配符的概念之后，很多朋友就很容易地写出这样的语句：

    OBJ = *.o
可能你的本意是：OBJ的值为所有的以.o为后缀的文件名列表，如果没有相应的.o，make就会通过make的隐式推导生成所有对应的.o文件。    

但是事实上并非如此，使用${OBJ}得出的值为 \*.o 字符串本身，是的，在这种情况下通配符并不起作用。  

那是不是在所有赋值的行为中，通配符都不起作用呢？博主在官方文档并没有找到相应的解答，但是一试便知。   

在当前目录下执行ls：

    main.c foo.c bar.c Makefile

Makefile的内容为：

    1 OBJ = *.c
    2 main:
    3     @echo ${OBJ}

执行 make 时，结果为：

    bar.c foo.c main.c  

这个结果说明，**从原理上来说，当你在赋值时指定通配符匹配时，如果通配符表达式匹配不到任何合适的对象，通配符语句本身就会被赋值给变量**。所以，在上面的示例中，在当前目录下不存在.o文件时，${OBJ}就被赋值为"*.o"这明显不是我们想要的。而"*.c"因为指定目录下有.c文件而被正常赋值。    


### 通配符函数
make作为一个成熟的工具，既然出现了问题，自然是有对应的解决方案的，我们可以使用通配符函数**wildcard**来实现上面的问题。

    OBJ = ${wildcard *.o}
OBJ的赋值指定使用通配符的扩展方式，这样即使是没有匹配到任何合适的文件，${OBJ}的内容为空，而并非是错误的"*.o"。  



## 规则中的特殊变量
在makefile的编译规则中，有一些特殊的內建变量，下面就列出一些常用的內建变量：
为了演示方便，这里再回顾一下makefile目标的编译规则：

    目标：依赖列表
        命令

* **$@**：表示需要被编译的目标 
* **$<**：依赖列表中第一个依赖文件名 
* **$^**：依赖列表中所有文件
* **$?**: 依赖文件列表中所有有更新的文件，以空格分隔
* **~** 或者 **./** ：用户的家目录,如果 **~**后接字符串，表示/home/+字符串,比如~downey,展开为/home/downey/。


## 模式规则
模式规则类似于普通规则。只是在模式规则中，目标名中需要包含有模式字符"%"，包含有模式字符"%"的目标被用来匹配一个文件名，"%""可以匹配任何非空字符串。规则的依赖文件中同样可以使用"%"，依赖文件中模式字符"%"的取值情况由目标中的"%"来决定。例如：对于模式规则"%.o : %.c"，它表示的含义是：所有的.o文件依赖于对应的.c文件。  

下列示例就是一个makefile內建的模式规则，由所有的.c文件生成对应的.o文件：

    %.o : %.c
        $(CC) -c $(CFLAGS) $< -o $@
    
根据这个模式规则，makefile提供了隐式推导规则。   

同时，模式规则的依赖可以不包含"%"，当依赖不包含"%"时代表的是所有与模式匹配的目标都依赖于指定的依赖文件。  

### 静态模式规则
静态模式可以更加容易地定义多目标的规则，它的语法是这样的：

    目标 ...: 目标模式 ： 依赖的模式
            命令
            ...
相对于普通的模式规则，静态模式规则则显得更加地灵活，作为模式规则的一种，仍然使用"%"来进行模式的匹配，我们来看下面一个简单的例子：

当前目录下的文件：foo.c foo.h bar.c bar.h main.c.
makefile内容：

    1 OBJ = foo.o bar.o
    2 main:${OBJ}
    3     cc ${OBJ} main.c -o main
    4 ${OBJ}:%.o : %.c
    5     cc -c $^ 
执行make时的运行log:

    cc -c foo.c 
    cc -c bar.c 
    cc foo.o bar.o main.c -o main
make在编译时会将执行的指令打印出来，这一部分就是实际被执行的指令。  
可以看到，在makefile第二行，**main**的依赖文件为${OBJ},即**foo.o**和**bar.o**,make在当前目录中并没有找到这两个文件，所以就需要寻找生成这两个依赖文件的规则。

第四行就是生成**foo.o**和**bar.o**的规则，所以需要先被执行，这一行使用了静态模式规则，对于存在于${OBJ}中的每个.o文件，使用对应的.c文件作为依赖，调用命令部分，而命令就是生成.o文件。  

可以看到，相对应普通的模式规则，静态模式规则相对来说更加地灵活。  

### 另一种常用的语法
在模式规则时还有另一种常用的语法，是这样的：

    ${OBJ：pre-pattern=pattern}

举个例子：
    ${OBJ:%.c=%.o}
    
这条语句的作用是：将OBJ中所有.o后缀文件替换成.c后缀文件。   


### 通配符与模式规则区别
乍一看，通配符和模式匹配像是同一个东西， * 和 % 都表示匹配任意的对象。  
当然，这种"乍一看"的印象是错误的。

模式匹配对应的是生成规则，规则对应：目标、依赖和命令，与普通规则不同的是，它并不显示地指定具体的规则，则是自动匹配。   

而通配符对应的是目标，表示寻找所有符合条件的目标。  

一个是针对执行规则，一个是针对目标文件，自然是不同的。  

(模式规则和函数中的模式匹配也是不同的)。  
## 学以致用
在makefile中，通配符与模式规则运用灵活，功能强大，同时也带来的一定的应用难度，合理地运用这些特性可以事半功倍。

我们来看看下面这个简单的、单目录模式下编译可执行文件的makefile模板：

**环境**：参与编译的文件：

    foo.c  foo.h  bar.c  bar.h  common.h main.c
    foo.c 依赖 foo.h common.h
    bar.c 依赖 bar.h common.h
    main.c为主文件，依赖foo.h  bar.h  common.h

**目标**：编译可执行文件main.


**makefile**：

    1 SRC = ${wildcard *.c}
    2 
    3 MAIN_SRC = main.c
    4 
    5 TARGET = main
    6 
    7 RAW_OBJ = ${patsubst %.c,%.o,${SRC}}
    8 
    9 OBJ = ${filter-out main% ,${RAW_OBJ}}
    10 
    11 ${TARGET}:${OBJ}
    12     cc $^ ${MAIN_SRC} -o ${TARGET}
    13 
    14 ${OBJ}:%.o : %.c %.h common.h
    15     cc -c $^
    16 
    17 clean:
    18     rm -rf *.o ${TARGET}

键入以下指令就可以开始工程的编译：

    make
### 示例讲解

* 第一行：将所有的.c为后缀的文件名赋值给SRC，以空格分隔。结果为：\${SRC} = main.c foo.c bar.c  

* 第三行：赋值MAIN_SRC为main.c  

* 第五行：指定编译目标，即可执行文件的变量  

* 第七行：使用patsubst()函数将SRC中所有.c后缀文件转换成.o后缀，并赋值给RAW_OBJ，patsubst是一个模式匹配的字符串匹配函数(见后面的章节)。此时\${RAW_OBJ} = main.o foo.o bar.o  

* 第九行：因为不需要编译生成main.o,所以需要使用filter-out()函数去掉main.o，filter-out()函数的作用是反选(见后面的章节)，此时${OBJ} =foo.o bar.o   

* 第11-12行：这是makefile中的第一个目标，执行make时默认执行这个目标，它依赖\${OBJ},但是目前\${OBJ}中.o文件没有生成，所以需要先调用14行的静态规则语句生成所有的.o文件，然后再与main.c一起编译生成可执行文件main。   

* 第14-15行：静态模式，由.c文件生成对应的.o文件，**这里需要特别强调的一点是依赖文件，很多朋友总是会忽略依赖文件的作用，因为依赖文件在编译的时候并不提供任何帮助，但是make需要靠依赖文件来判断文件的更新以至于判断目标是否需要更新，如果忽略依赖文件，仅仅添加%.c的话，*.h common.h中的修改将不会导致重新编译，这样明显不是用户想要的。**  

* 第17-18行：clean目标，将生成的文件全部删除。调用方法是make clean。  


***  
***    
***  

参考资料：[官方文档](https://www.gnu.org/software/make/manual/make.html)

***  
好了，关于 《深入解析Makefile系列(2) -- 常用的通配符、內建变量和模式规则》 的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.








