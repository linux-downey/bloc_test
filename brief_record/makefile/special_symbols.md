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
在设置变量时通常不会发生通配符的扩展


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








官方资料：https://www.gnu.org/software/make/manual/make.html


















