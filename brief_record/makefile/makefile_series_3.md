# 深入解析Makefile系列(2) -- 函数的使用
在之前的章节中，我们讲解了编写makefile的基本规则，在这一章节中我们将讨论makefile规则中的函数使用。  

## 语法
函数的使用语法是这样的：

    $(function arguments)
    或者
    ${function arguments}
参数之间用逗号","分隔，单个参数可以是以空格分隔的列表。  

在makefile中，有一系列的內建函数以适用于各类文件的处理，函数本身就是对操作过程的一种封装，使用官方提供的函数能大大提高我们编写makefile的效率，降低复杂操作的出错率。接下来我们就来看看一些常用的函数。  

对于复杂难以理解的函数，博主将附上详细的注释和示例。对于简单的函数，则不需要添加示例。

## 字符串相关

### $(subst from,to,text) 
#### 文本(字符串)替换

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

### $(patsubst pattern,replacement,text) 
#### 文本(字符串)模式替换
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

这个操作可以使用一个更加简单的模式匹配语法来操作，相当于：

    ${TEXT : %.c=%.o}
结果是一样的。  


### $(strip string)
#### 字符串精简
**函数介绍：**
* 函数作用：将字符串中前导和尾随的空格删除，在字符串中存在多个连续空格时，使用一个空格替换。  
* 参数：
    * string ： 目标字符串
* 返回值 ： 精简完的字符串

**示例**：(无)


### $(findstring find,in)
#### 在主文本中寻找子文本
**函数介绍：**  
* 函数作用 ： 顾名思义，这是文本查找函数，在in中寻找是否有find文本。
* 参数：
    * find : 子文本
    * in ： 主文本
* 如果in中存在find，返回find，否则返回""(空文本)。

""示例""：(无)

### $(filter pattern…,text) 和 $(filter-out pattern…,text)
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

### $(sort list)
**函数介绍：**  
* 函数作用 ：将给定的list(通常是以空格分隔的文件列表)按照首字母字典排序。
* 参数：
    * list ： 目标列表
* 返回值 ： 返回排序后的列表。  

**示例：**(无)


### $(word n,text)
**函数介绍：**  
* 函数作用 ：返回text列表中第n个元素，通常来说，这个text为文件或字符串列表，元素为单个的文件名或字符串
* 参数：    
    * n ： 第n个元素，比较特殊的是，元素列表的计数从1开始。
    * text : 文件列表
* 返回值 ： 返回第n个元素

**示例：**

    TEXT := foo.c foo.h bar.c
    RESULT := ${word 2,${TEXT}}

${RESULT}结果：  

    foo.h

值得注意的是，当n超出list范围时，返回空字符串。当n小于1时，make将报错。  

***
### $(wordlist s,e,text)
**函数介绍：**  
* 函数作用 ：返回text列表中指定的由s(start)开始由e(end)结尾的列表，s和e均为数字。    
* 参数：
    * s ： 截取开始的地方，s从1开始
    * e ： 截取字符串结束的地方。
    * text ： 目标文件列表
* 返回值 ： 返回截取的字符串

**示例：**(无)

需要注意的是：
* 如果s大于text的最大列表数量，返回空字符串
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
* 当list1和list2不等长时，视为与空字符串的链接，例如list1为3个元素，list2有两个元素，那么list1的第三个元素对应空字符串。    
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

**示例：**(无)

    TEXT := foo.c bar.c
    RESULT := ${foreach file , TEXT , ${file}:%.o=%.c}













+