# makefile当中的一些特殊符号


-开头的命令


环境变量MAKEFILES:当前环境变量的设置是一串由空格隔开的makefile名称，作用类似于include，在读取当前makefile之前，make将会先读取MAKEFILES变量中的每一个文件。  

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




























官方资料：https://www.gnu.org/software/make/manual/make.html



















makefile最简单的示例以及原则
初级：编译，添加依赖，生成文件，介绍最基本的编译条件，基本命令
初中级：makefile的隐式推导规则，伪目标、变量的赋值、行的分隔
中级：使用通配符，命令详解
高级：函数的使用和介绍





多目录makefile加一章
函数加一章









