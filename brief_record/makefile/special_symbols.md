# makefile当中的一些特殊符号

$@ $> $^
$* 不包含扩展名的目标文件名称

?= := +=

-开头的命令

隐含规则：
通常使用的模式规则：

%.c:%.o
	$(CC) $(CCFLAGS) $(CPPFLAGS) -c -o $@ $<
将所有的.c文件替换成.o文件。  



make会把Makefile中第一个target作为目标执行(不包括以.开始的伪目标)
目标的格式为：

target:dependencies
	command
必须以tab作为第二行的分隔符

使用命令行命令，需要添加@。  
比如@echo hello

使用变量，变量的赋值需要用$(),或者${}
变量的另一种初始化方式：
VAR = ${VAR:VAL}  如果VAR有值，就不变，如果VAR为空，就赋值为VAL.

让make对目标进行推导：
列出.o文件，make就会默认使用同名的.c文件进行编译。

比如：
	OBJ += test1.o test2.o
	
	target : ${OBJ}
	    ar -rsc liba.a ${OBJ}
	test1.o : test1.h common.h
	test2.o : test2.h common.h
	
或者使用另一种格式的自动推导方式。

	OBJ += test1.o test2.o
	
	target : ${OBJ}
	    ar -rsc liba.a ${OBJ}
	${OBJ}:common.h
	test1.o : test1.h
	test2.o : test2.h

伪目标：
.PHONY : clean
clean:
	...
如果一个目标没有依赖文件，且不作为第一个target放置，它就不会被执行。

通常情况下，只要clean不作为第一个target放置的话，可以省略.PYONY,但是，如果在编译时需要目标文件clean的情况下，就会产生冲突，.PHONY表示当前clean是一个伪目标。  

一个Makefile包含什么：
* 显示的规则指导文件的编译
* 隐式规则指导文件的编译
* 变量的定义
* 一些特殊的指令，包括读取其他makefile文件，条件编译、以其他方式定义变量
* '#'在前的一行表示注释
* 反斜杠换行显示，Makefiel的语法是以行为单位的。



makefile可以include其他文件，包括其他的makefile，其他的bash文件。如果被include的文件不存在，不会报错。    
当make遇到include指令时，先去其他文件读取被include的文件，然后接着回来处理当前makefile的指令。  
可以将某些编译操作分开，实现分布式的编译。

环境变量MAKEFILES:当前环境变量的设置是一串由空格隔开的makefile名称，作用类似于include，在读取当前makefile之前，make将会先读取MAKEFILES变量中的每一个文件。  

-I 或者 --include-dir用作指定make的搜索路径。  先搜索-I指定的目录，然后再搜索系统目录。  

覆盖其他makefile中的某一部分：没太看懂，需要重新梳理。  

make是怎么读取makefile的：
第一阶段：读取所有的makefile文件，读取所有变量，构建依赖图
第二阶段：根据makefile的条件，决定怎么执行makefile。
这两个阶段区别于脚本，需要先定义在使用，而是先扫一遍所有变量。  


+= 表示append ， !=表示

关于makefile中变量是立即分配还是延期分配：
 =     延期执行
 ?=    延期执行
 :=    立即分配
 ::=   立即分配
 +=    延期或者立即分配(如果之前是:=或者::=分配的，就是立即分配，否则延期分配)
 !=    立即分配


两种类型的依赖文件：
正常的依赖文件：当依赖文件有任何改变时，target也要重新编译
order-only依赖文件：当依赖文件有改变时，并不重新编译target，在依赖项处使用管道符，管道左边为正常依赖文件，管道右边为order-only依赖文件。  


通配符的使用：
makefile中支持的通配符为 * ? [...],使用方式和shell中是一样的。  
~或者~/表示家目录,
如果~后面接字符串，表示/home+字符串，比如~downey，表示/home/downey/
$? 在shell中表示命令执行的返回值，而在makefile中可以表示已经修改过的依赖文件列表。  


通配符在变量分配时并不起作用，如果这样写：这算是一个通配符的陷阱，需要注意
obj = *.o，就是创建了一个\*.o的变量
需要这么写：
obj := ${wildcard *.o} 
在设置变量时通常不会发生通配符的扩展，如果需要在设置变量时使用通配符，就需要加上wildcard关键词。


在windows下运行makefile时，也需要注意，windows文件路径使用\隔开，而\在makefile中表示转义，这里需要注意。

如果要取消通配符的作用，就在它之前加一个反斜杠，相当于转义。

函数通配符：
objects := $(patsubst %.c,%.o,$(wildcard *.c))
patsubst在这里的作用就是将所有.c转化为.o

搜索目录：
VPATH环境变量，这个环境变量的值是一个目录列表。

vpath是一个设置VPATH环境变量的命令，
vpath pattern directories    设置目录
vpath pattern                删除匹配pattern的目录
vpath                        删除所有设置的目录
通常pattern使用%来匹配，%表示匹配任意的。
makefile中的%匹配所有的目标。  

目录搜索执行的过程：
* 目标文件不存在，启动目录搜索
* 如果搜索到文件，保留该路径，并且暂时将该文件存储为目标
* 使用相同的方法检查此目标的所有依赖
* 如果目标不需要重建，保留搜索的路径，如果需要重建，则在当前路径下重建而不是到搜索到的路径汇总重建。  

目录搜索的隐式规则：如果碰到一个foo.o没有在当前目录中找到foo.c文件，搜索其他目录，如果在搜索过程中发现foo.c，同样可以支持编译foo.o。 

链接库的搜索路径：
当指定-lxxx时，先搜索libxxx.so，然后libxxx.a是否存在于当前目录，然后再根据VPATH去搜索其他目录。  
 .LIBPATTERNS指定了搜索名称，一般来说， .LIBPATTERNS的内容就是lib%.so lib%.a表示在-l后指定的xxx时，搜索libxxx.a/so

 
$@表示目标文件，$^表示所有的依赖文件，$<表示第一个依赖文件。


伪目标：
使用.phony来指定一个或多个伪目标
伪目标可以有依赖，伪目标并非为一个真实需要生成的目标文件，
显示指定伪目标的原因是：
避免文件名的冲突，如果有一个正常目标名为clean，有一个伪目标为clean，就会冲突
提高makefile的编译效率，如果一个伪目标不显示指定，make就会试图去将它当成

如果一条指令没有依赖或者没有编译指令，make会认为它每次都要被执行。  
就像make clean。

特殊的makefile內建变量：
.PHONY
.SUFFIXES
.DEFAULT
.INTERMEDIATE
.SECONDARY
.SECONDEXPANSION
.DELETE_ON_ERROR
.LOW_RESOLUTION_TIME
.SILENT
.EXPORT_ALL_VARIABLES
.NOTPARALLEL
.ONESHELL
.POSIX

多个目标共用一个rule
a.o b.o : common.h


静态模式规则的语法：
target ```: target-pattern: prereq-patterns
例如：
obj = foo.o bar.o
all: ${obj}
${obj}:%.o:%.c
	command
这种语法表示target-pattern是怎么由prereq-patterns生成。一般使用模式匹配%。

双冒号：：
双冒号的作用就是为同一个目标建立不同的规则，各自单独的编译

Newprog :: foo.c

       $(CC) $(CFLAGS) $< -o $@

Newprog :: bar.c

       $(CC) $(CFLAGS) $< -o $@
当foo.c更新时，Newprog更新，当bar.c更新时，Newprog更新。如果是普通规则下的：，make将会报错。双冒号规则需要定义命令。


命令的格式：
命令的解析，本质上命令是由shell来解析的
* 以tab开头的空白行并不是空白行，而算是一个空命令
* 在命令中添加的注释会被直接传递给shell，makefile命令中的注释是否为注释取决于shell。
* 在命令中定义的变量，即tab后的第一个字符串将被视为命令的一部分，而不是makefile中的变量定义，并传递给shell
* 条件语句会被传递给shell

关于行的拆分：在命令中，行的拆分也遵循shell的处理风格。




变量的使用：
在命令中使用makefile中的变量需要加$符号。  

变量的两种处理方式：立即分配和延迟分配

foo = $(bar)
bar = $(ugh)
ugh = Huh?

all:echo $(foo)

输出的结果是 Huh?

延迟分配会扫描整个makefile最后再决定变量的值，
它有一个坏处就是不能这么写：

	CFLAGS = ${CFLAGS} -O
这样的添加会导致无限循环。


替代策略：
	foo := a.o,b.o
	bar := (foo:%.o=%.c)
bar的值被赋值为a.c,b.c。相当于一个临时转换，而foo的值不会被改变。  

变量名的计算，处理出现嵌套的变量：示例：

	x = y
	y = z
	a := $($(x))
$x = y，y依旧是一个变量，所以需要使用$($(x))来获取y的值。

变量赋值的手段：
* 在运行make的时候可以指定覆盖一个变量的值

！= 右边表示执行一个shell命令，返回的结果赋值给左值。 
+= 表示追加左值列表。

override命令：override命令具有最高的优先级，随后出现的赋值操作符会被忽略：
	override variable = value

define指令：defile与endef可以声明一个带换行的变量。  
undefine变量：使用
undefine


特定目标变量：在target的依赖处可以定义变量，但是这个变量是自动变量，不具有全局性。

prog : CFLAGS = -g
prog : prog.o foo.o bar.o


指定模式下的变量:

pattern … : variable-assignment
%.o : CFLAGS = -O

抑制变量的继承：
在命令部分使用与全局变量同名的局部变量时，局部变量会继承全局变量的值，如果不想继承这个值，可以使用
	private CFLAGS = ...

特殊变量：6.14节



条件控制部分：
ifeq/ifneq
else
endif

ifdef val
endif
只要val非空，条件就成立，即使val的值为false

bar =
foo = $(bar)
ifdef foo
frobozz = yes
else
frobozz = no
endif
这种情况下，frobozz的值为yes，因为${foo}为bar，即使${bar}为空

foo =
ifdef foo
frobozz = yes
else
frobozz = no
endif
这种情况下，frobozz的值为no


函数部分：
函数调用的语法：
${function arguments ...}

一些常用的函数：
${subst from,to,text}
字符串的替换，将text中的from替换成to，每一处都进行替换。  
其中，text可以是以空格隔开的列表。

${patsubst pattern,replacement,text}
字符串的模式替换，例如：

$(patsubst %.c,%.o,x.c.c bar.c)
结果是生成 x.c.o,bar.o
这个函数的效果其实等同于：${var:%c=%o}

$(strip string)
移除字符串开头和结尾的空格

$(findstring find,in)
搜索in列表中是否有find字符串，如果有，返回find，如果没有，返回in。  

$(filter pattern…,text)
返回text中匹配的字符列表。  

$(filter-out pattern…,text)
返回与filter相反的字符列表

$(sort list)
以单词首字母给list中各单词排序

$(word n,text)
返回text中的第n个单词,n的有效值从1开始。

$(wordlist s,e,text)
返回从s开始，e结束的单词，s和e为数字，s的有效值从1开始。 相当于截取text中某一段

$(words text)
返回text中的单词数量

 $(word $(words text),text)
 返回

$(firstword names…)
返回第一个单词

$(lastword names…)
返回最后一个单词


文件名操作相关的函数：

$(dir names…)
提取文件名对应的目录路径
	$(dir src/foo.c hacks)
结果是src/ ./’	

$(notdir names…)
提取带目录路径文件名中的文件名，即删除目录路径部分。

$(suffix names…)
提取文件名的后缀，返回后缀列表

$(basename names…)
与提取后缀作用相反

$(addsuffix suffix,names…)
给names中的列表逐一添加后缀

$(addprefix prefix,names…)
给names中的列表逐一添加前缀

$(join list1,list2)
逐一地连接list1和list2中对应的单词
例如：
	$(join a b,.c .o)’ 返回 ‘a.c b.o’.


$(wildcard pattern)
返回匹配pattern的文件名，wildcard表示通配符

$(realpath names…)
返回names中的绝对地址，不包含链接

$(abspath names…)
返回names中的绝对地址，不处理软链接

条件处理函数：
if
and
or
在$(if ...)时被作为三个函数处理

$(if condition,then-part[,else-part])
如果condition为真，执行then-part,否则执行else-part
$(or condition1[,condition2[,condition3…]])
当前面的condition执行不成功时，才执行后面的condition
$(and condition1[,condition2[,condition3…]])
顺序执行condition，只有前面执行成功时才继续执行后面指令，
判断执行成功的标准是返回非空字符串


foreach：foreach更像是C中的for，原型：
$(foreach var,list,text)
var表示临时变量，list表示目标文件列表，text其实更像是command，表示对每个var执行的命令，并返回执行结果

file函数：
$(file op filename[,text])
op表示操作：可以是>覆盖写    >>追加写 和    <读
filename可以是列表

call函数：
$(call variable,param,param,…)

call可以定义一个函数，函数的参数在定义时使用$(1)、$(2)等代替，比如一个最简单的例子
func = echo $(1) $(2)
result = ${call func,x,y}
第一行表示定义一个函数func，第二行表示调用，传入参数x和y。

如果定义的函数名与内建的函数名同名，在调用时总是调用内建的函数名

value函数：
$(value variable)
value直接接一个变量名，返回变量的最后的值，而不管其中有多少层变量之间的嵌套，不用添加$。

origin函数：
$(origin variable) 
origin函数并不对variable做任何操作，而是返回变量的信息，返回这个变量是怎么被定义的。
undefined、default、environment、environment override、file、command line、override、automatic

flavor函数：
$(flavor variable)
flavor返回变量被定义的方式；
undefined、recursive、simple


控制makefile行为的函数：

$(error text…)
生成一个fatal error，返回text的错误信息。
$(warning text…)
生成给一个警告信息
$(info text…)
生成一个消息。


shell函数：
表示调用一个shell命令



make的运行：
make的运行结果：
0   --  运行成功
2   --  遇到错误
1   --  使用了-q选项，但是某些目标并不是最新的，-q不运行命令，只检查文件的更新


make -f选项指定对应的makefile文件

默认情况下，执行没有参数的make将第一个目标作为编译目标
make 可以指定目标编译，比如make clean
.DEFAULT_GOAL同样可以指定目标
另一个变量MAKECMDGOALS 在执行make命令时会被赋值为将被执行的目标。  



























官方资料：https://www.gnu.org/software/make/manual/make.html



















makefile最简单的示例以及原则
初级：编译，添加依赖，生成文件，介绍最基本的编译条件，基本命令
初中级：makefile的隐式推导规则，伪目标、变量的赋值、行的分隔
中级：使用通配符，命令详解
高级：函数的使用和介绍





多目录makefile加一章
函数加一章









