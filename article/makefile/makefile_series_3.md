# 深入解析Makefile系列(3) -- 函数的使用
在之前的章节中，我们讲解了编写makefile的基本规则，在这一章节中我们将讨论makefile规则中的函数使用。  

## 语法
函数的使用语法是这样的：

    $(function arguments)
    或者
    ${function arguments}
参数之间用逗号","分隔，单个参数可以是以空格分隔的列表。  

在makefile中，有一系列的內建函数以适用于各类文件的处理，函数本身就是对操作过程的一种封装，使用官方提供的函数能大大提高我们编写makefile的效率，降低复杂操作的出错率。接下来我们就来看看一些常用的函数。  

对于复杂难以理解的函数，博主将附上详细的注释和示例。对于简单的函数，则不需要添加示例。

## 文本相关

### $(subst from,to,text)   
#### 文本(文本)替换

**函数介绍：**  
* 函数作用：对目标文本(或列表)text执行文本替换，将主文本中的from替换成to，并返回替换后的新文本。  
* 参数：
    * from ： 将要被替换的子文本
    * to   ： 替换到文本text的新子文本
    * text ： 被操作的主文本
* 返回：返回替换之后的新文本

**示例**:

    1 TEXT = "hello world" "hello downey"
    2 FROM = hello
    3 TO   = HELLO
    4 RESULT = $(subst $(FROM),$(TO),$(TEXT))
${RESULT}结果：

    HELLO world HELLO downey
可以看到，如果text是文本列表一样支持。  

***  
### $(patsubst pattern,replacement,text) 
#### 文本(文本)模式替换
**函数介绍:**
* 函数作用：对目标文本(或列表)text执行文本替换，以模式替换的形式进行替换。  
* 参数：
    * pattern ： 将要被替换的模式匹配方式.
    * replacement ： 替换后的模式匹配方式.
    * text : 被操作的文本
* 返回：返回替换后的新文本

**示例**：

    TEXT = foo.c bar.c
    RESULT = $(patsubst %.c,%.o,${TEXT})
${RESULT}结果：

    foo.o bar.o

同样的，TEXT可以是列表。  

这个操作可以使用一个更加简单的模式匹规则语法来操作，相当于：

    ${TEXT : %.c=%.o}
结果是一样的。  

***  

### $(strip string)
#### 文本精简
**函数介绍：**
* 函数作用：将文本中前导和尾随的空格删除，在文本中存在多个连续空格时，使用一个空格替换。  
* 参数：
    * string ： 目标文本
* 返回值 ： 精简完的文本

**示例**：(无)
***  

### $(findstring find,in)
#### 在主文本中寻找子文本
**函数介绍：**  
* 函数作用 ： 顾名思义，这是文本查找函数，在in中寻找是否有find文本。
* 参数：
    * find : 子文本
    * in ： 主文本
* 如果in中存在find，返回find，否则返回""(空文本)。

""示例""：(无)

***  

### \$(filter pattern…,text) 和 \$(filter-out pattern…,text)
**函数介绍：**  
* 函数作用 ： 过滤作用，将符合模式规则的文本挑选出来。  
* 参数：
    * pattern  过滤的模式规则
    * text  将要处理的文本
* 返回值 ： 返回符合模式规则的文本

**示例：**  

    TEXT := foo.c bar.c foo.h bar.h
    RESULT = $(filter %.c,$(TEXT))
${RESULT}结果：

    foo.c bar.c

filter-out()函数的作用与filter相反，运用模式规则进行反选，返回反选的结果。  

***  

### $(sort list)
**函数介绍：**  
* 函数作用 ：将给定的list(通常是以空格分隔的文件列表)按照首字母字典排序。
* 参数：
    * list ： 目标列表
* 返回值 ： 返回排序后的列表。  

**示例：**(无)

***  

### $(word n,text)
**函数介绍：**  
* 函数作用 ：返回text列表中第n个元素，通常来说，这个text为文件或文本列表，元素为单个的文件名或文本
* 参数：    
    * n ： 第n个元素，比较特殊的是，元素列表的计数从1开始。
    * text : 文件列表
* 返回值 ： 返回第n个元素

**示例：**

    TEXT := foo.c foo.h bar.c
    RESULT := ${word 2,${TEXT}}

${RESULT}结果：  

    foo.h

值得注意的是，当n超出list范围时，返回空文本。当n小于1时，make将报错。  

***
### $(wordlist s,e,text)
**函数介绍：**  
* 函数作用 ：返回text列表中指定的由s(start)开始由e(end)结尾的列表，s和e均为数字。    
* 参数：
    * s ： 截取开始的地方，s从1开始
    * e ： 截取文本结束的地方。
    * text ： 目标文件列表
* 返回值 ： 返回截取的文本

**示例：**(无)

需要注意的是：
* 如果s大于text的最大列表数量，返回空文本
* 如果e大与text的最大列表数量，返回从s开始到结尾的列表
* 如果s大于1，返回空。  


***
### $(words text)
**函数介绍：**  
* 函数作用 ：返回text列表中的元素数量
* 参数：
    * text ： 目标列表
* 返回值 ： 返回text列表中的元素数量

**示例：**(无)

***
### $(firstword names…)  \$(lastword names…)
**函数介绍：**  
* 函数作用 ：返回names列表中的第一个元素
* 参数：
    * names ： 目标列表
* 返回值 ： 返回names列表中的第一个元素

**示例：**(无)

lastword()函数与firstword相反，返回最后一个元素
***  

## 文件与目录操作函数

### $(dir names…) \$(notdir names…)
**函数介绍：**  
* 函数作用 ：截取文件路径中的目录部分，如：/home/downey/file.c，截取/home/downey/，目标可以是列表
* 参数：
    * names ： 目标文件，可以是列表 
* 返回值 ： 返回目录，如果目标是列表，返回以空格分隔的目录。  

**示例：**(无)

**与dir相对的,notdir()仅截取文件名**

***
### $(suffix names…)  \$(basename names…)
**函数介绍：**  
* 函数作用 ：获取文件列表中的后缀部分。
* 参数：
    * names ： 目标文件，可以是列表 
* 返回值 ： 返回文件列表中的后缀部分。如：".c",".o"

**与suffix相对的,basename()去除后缀名**

**示例：**(无)

***
### $(addsuffix suffix,names…)
**函数介绍：**  
* 函数作用 ：为目标文件列表添加后缀
* 参数：
    * suffix ： 添加的后缀内容
    * names ： 目标列表
* 返回值 ： 返回添加完后缀的列表

**示例：**

    TEXT := foo bar
    RESULT := ${addsuffix .o , ${TEXT}}

${RESULT}结果:

    foo.o bar.o

***  

### $(join list1,list2)
**函数介绍：**  
* 函数作用 ：逐个地将list2中的元素链接到list1。  
* 参数：
    * list1 ： 链接后元素在前的列表
    * list2 ： 链接后元素在后的列表
* 返回值 ： 返回链接后的链表

**示例：**

    LIST1 := foo bar
    LIST2 := .c  .p
    RESULT := ${join ${LIST1} , ${LIST2}}

${RESULT}结果:
    foo.c bar.p

有两点需要注意的地方：  
* 当list1和list2不等长时，视为与空文本的链接，例如list1为3个元素，list2有两个元素，那么list1的第三个元素对应空文本。    
* list中的空格将始终被当做一个空格处理。  

***  
### $(realpath names…)  
**函数介绍：**  
* 函数作用 ：对names中的每个文件，求其绝对路径，当目标为链接时，将解析链接。  
* 参数：
    * names ：目标文件名(列表) 
* 返回值 ： 目标文件名对应的绝对路径(列表)。 

**示例：**(无)
***  

### $(abspath names…)
**函数介绍：**  作用与realpath()函数相似，唯一的不同是不解析链接。  
***  


## 其他常用函数

***    
### $(foreach var,list,text)

**函数介绍：**  
* 函数作用 ：对list中的每个var，调用text命令。  
* 参数：
    * var ：被操作的目标元素  
    * list ： 目标元素列表  
    * text ： 对目标元素执行的操作。  
* 返回值 ： 返回执行操作后的文本.

**示例：** 

    TEXT := foo.c bar.c
    RESULT := ${foreach file,${TEXT},${file}.c}

${RESULT}结果:

    foo.c.c bar.c.c

从结果可以看出，makefile中在${file}后添加了一个.c后缀。    

**值得注意的是：第三个参数中的text是对元素执行的操作，但是！这里的操作并不是指的shell下的操作，不能执行类似于：@echo ${file}等操作，echo是shell下的函数，而这里是遵循makefile的语法，我们可以使用makefile中允许的函数比如：wildcard()，我们应该分清这一点。**  
**在示例中，对于TEXT中的每个file(这个可以是任何临时变量)，返回第三个参数执行后的文本，这里是直接将file展开加上.c，整个表达式最后的返回结果就是所有单条语句返回结果的列表集合。**   

*** 



### $(file op filename[,text])

**函数介绍：** 
* 函数作用 ：向文件执行文本的输入输出  
* 参数：
    * op ： 要对文件进行的操作，支持：>(覆盖写) >>(追加写)  <(读)
    * filename ： 文件名  
    * text ： 如果op为写，此参数表示要写入的文本。    
* 返回值 ： 返回值无意义
**示例：** 

    TEXT := "hello world"
    RESULT := \${file > test,${TEXT}}

执行结果：当当前目录下存在test文件时，"hello world"被写入到test中，当不存在test文件时，文件被创建且同时写入"hello world".  

***

### $(call variable,param,param,…)
**函数介绍：**
* 函数作用 ：call函数在makefile中是一个特殊的函数，因为它的作用就是创建一个新的函数。你可以使用call函数创建各种实现复杂功能的函数。
* 参数：
    * variable ： 函数名
    * param ： 函数参数,在使用是被传递给自定义的variable函数。
    * param ...   
* 返回值 ： 返回定义函数的结果.  

当call函数被调用时，make将展开这个函数，函数的参数会被赋值给临时参数 \$1,\$2,\$0 则代表函数名本身，参数没有上限也没有下限。     

对于call函数需要注意几点：  
* 当定义的函数名与内建函数名同名时，总是调用内建函数。  
* call函数在分配参数之前扩展它们，意味着对具有特殊扩展规则的内置函数的引用的变量值可能不会正常执行。  

**示例：** 

    func = $1.$2
    foo = $(call func,a,b)
    all:
        @echo $(foo)
输出结果：

    a.b
可以看到，在第一行定义了函数的行为，即返回$1.$2,调用时参数参数a和b，结果为a.b。  

我们还可以使用define来定义更加复杂的函数：

    1 define func 
    2 $1.$2
    3 var = bar.c
    4 endef
    5 foo = $(call func,a,b)

foo的结果为：

    a.b
    var = bar.c
define是定义变量的一种方式，这种定义变量的特殊之处在于它可以定义带有换行的变量，所以它可以定义一系列的命令，在这里就可以定义一个函数。    




***
### $(value variable)

**函数介绍：** 
* 函数作用 ：获取未展开的变量值。    
* 参数：
    * variable ：目标变量，不需要添加"$"   
* 返回值 ： 返回变量的未展开的值.  
**示例：** 

    FOO = \$PATH
    all:
        @echo \$(FOO)
        @echo \$(value FOO)

执行结果：

    ATH
    /home/downey/bin:/home/downey/.local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin
第一个结果\${FOO}为ATH是因为，make将\$P解析成makefile中的变量，而不是将(PATH)作为一个整体来解析。  

第二个结果得到的就是真实的环境变量值。  

**事实上，博主对这个value函数的定义并不理解，按照官方文档理解，既然是未扩展的值，\$(value FOO)的返回值不应该是"$PATH"这个文本吗？**  
**之后，博主尝试了将PATH换成其他定义的变量，例如：**

    TEXT := hello
    FOO := $TEXT
    all:
        @echo $(value FOO)
**输出的结果为"EXT"(\$T+EXT)，而非hello(\${TEXT})，更不是"\$TEXT"这个文本，这个结果就非常奇怪了，明明这个和PATH一样同样是变量，为什么这个结果和$PATH结果完全不一样。**  

**是不是因为PATH是内置变量？那我就换一个内置变量试试：**

    FOO = $CURDIR
    all:
        @echo $(value FOO)
**输出结果为"URDIR",依旧是(\$C+URDIR)，\$(CURDIR)应该是当前目录。**  

**后来查看makefile的内置变量，发现PATH其实是linux shell环境下的全局变量,我接着使用USER(同是shell环境变量，使用export查看)，与PATH一样的效果：**  

    FOO = $USER
    all:
        @echo $(value FOO)
**输出为"downey"(我的用户名)**  
**其中FOO的赋值只能是 "=" 而不能是 ":="，这一部分确实是和扩展相关的属性。但是博主还是不理解其中的原理。**  
**经过各种实验，我只能将它理解为，value函数可以解析shell中的环境变量，而对于官方的说法：返回变量的未经扩展的值，目前仍然不能理解。**
**同时，网上的大部分博客都是复制粘贴，没有什么参考价值。**    
**希望路过的大神不吝赐教！！**

***


### $(eval text)

**函数介绍：** 
* 函数作用 ：eval在makefile中是一个非常特别的函数，它允许此函数新定义一个makefile下的结构，包含变量、目标、隐式或者显示的规则。eval函数的参数会被展开，然后再由makeifle进行解析。也就是说，eval函数会被make解析两次，第一次是对eval函数的解析，第二次是make对eval参数的解析。    
* 参数：
    * text ： 一个makefile结构    
* 返回值 ： 返回值无意义.
**示例：** 

    1 define func
    2 $1:
    3     cc $2 -o $$@
    4 endef
    5 
    6 $(eval $(call func,main,main.c))
执行结果：

    cc main.c -o main
在1-4行定义了一个变量集，我们也可以直接使用call函数，将其作为一个函数调用，返回：

    $(1):
        cc $(2) -o $$@
返回值依旧是一个makefile规则的表达式，这时候如果需要执行这个规则，那就要使用makefile的规则对其进行再一次地解析，就需要用到eval()函数，eval()函数通常与call()一起使用。   

至于第三行中出现的\$\$,在makefile的语法中，\$是一个特殊字符，通常与其他符号结合表示特定的含义，如果我们单纯的就想打出"\$"字符，我们需要使用"\$\$"表示"\$"符号，就像C语言中的转义符号'\'，如果我们要使用真实的'\'符号，我们就得使用'\\'来进行转义。  
***

### $(origin variable)

**函数介绍：** 
* 函数作用 ：这个函数与其他函数的区别在于，它不执行变量操作，而是返回变量的定义信息。  
* 参数：
    * variable ：被操作的目标变量   
* 返回值 ： 返回变量的定义信息。这些定义信息是一些枚举值：
    * undefined ： 变量未定义
    * default   ： 变量被默认定义，就像CC.
    * environment : 传入的环境变量
    * environment override ： 本来在makefile中被定义为环境变量，但是新定义的变量覆盖了环境变量。  
    * file      ： 在makefile中被定义
    * command line ： 在命令行中被定义
    * override   ： 使用override定义的变量
    * automatic  ： 在规则的命令部分被定义的自动变量，也可以理解为临时变量.  

**示例：** (无)  

***

### $(flavor variable)
**函数介绍：** 
* 函数作用 : 与origin的属性类似，这个函数返回变量的展开方式，在之前章节有提到过，变量展开方式有两种：循环递归扩展和简单扩展。  
* 参数：
    * variable ： 目标变量
* 返回值 ： 返回变量的展开方式，这是一个枚举值：
    * undefined ： 变量未定义
    * recursive ： 循环递归扩展
    * simple ： 简单扩展

**示例：** (无) 

***

## 控制执行流的函数

###  $(error text…)
**函数介绍：** 
* 函数作用 : 生成一个错误，并返回错误信息text。  
* 参数：
    * text ： 将要打印的错误信息。  
* 返回值：返回值无意义，makefile执行终止。  

**需要注意的是：只要调用了这个函数，错误信息就会生成，但是如果将其作为一个循环递归变量的右值，它会被最后扩展的时候才被调用，或者放在规则的命令部分，它将被shell解析，可能结果并不是你想要的。**

***  
### \$(warning text…) \$(info text…)
**函数介绍：**这两个函数属性与error相似，但是这两个函数并不会导致makefile的执行中断，而是警告和提示。  
***  

### $(shell command...)
**函数介绍：** 
* 函数作用 : shell函数与其他函数不一样的地方在于，它将它的参数当做一条shell命令来执行，shell命令是不遵循makefile的语法的，也就是说不由make解析，将传递给shell，这一点和规则中的命令部分是一样的。
* 参数：
    * command ： 命令部分
* 返回值 ： 返回shell命令执行结果  

**值得注意的是：make会对最后的返回值进行处理，将所有的换行符替换成单个空格。**  

在makefile中可以使用另一种做法来调用shell命令，就是用"`"(键盘左上角ESC下的键)将命令包含。同样的，"!="赋值运算符也是一种方法。    

***  
***    
***  

参考资料：[官方文档](https://www.gnu.org/software/make/manual/make.html)

***  
好了，关于 《深入解析Makefile系列(3) -- 函数的使用》 的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.


