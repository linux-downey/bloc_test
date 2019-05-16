# makefile当中的一些特殊符号

$@ $> $^
$* 不包含扩展名的目标文件名称


-开头的命令


%.c:%.o
	$(CC) $(CCFLAGS) $(CPPFLAGS) -c -o $@ $<
将所有的.c文件替换成.o文件。  


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



两种类型的依赖文件：
正常的依赖文件：当依赖文件有任何改变时，target也要重新编译
order-only依赖文件：当依赖文件有改变时，并不重新编译target，在依赖项处使用管道符，管道左边为正常依赖文件，管道右边为order-only依赖文件。  


通配符的使用：
如果~后面接字符串，表示/home+字符串，比如~downey，表示/home/downey/
  



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




反斜杠在unix和windows的区别。


























官方资料：https://www.gnu.org/software/make/manual/make.html



















makefile最简单的示例以及原则
初级：编译，添加依赖，生成文件，介绍最基本的编译条件，基本命令
初中级：makefile的隐式推导规则，伪目标、变量的赋值、行的分隔
中级：使用通配符，命令详解
高级：函数的使用和介绍





多目录makefile加一章
函数加一章









