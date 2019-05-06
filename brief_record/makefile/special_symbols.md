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

makefile的名称约定：当执行make时，make程序会在当前目录下寻找GNUmakefile，makefile、Makefile这三种文件。  
同时，如果用户不想用标准名称，可以使用-f或者--file参数指定文件名作为Makefile。 


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




makefile中的%匹配所有的目标。  








官方资料：https://www.gnu.org/software/make/manual/make.html


















