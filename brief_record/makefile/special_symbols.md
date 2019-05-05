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
	3 
	4 target : ${OBJ}
	5     ar -rsc liba.a ${OBJ}

或者使用另一种格式的自动推导方式。



官方资料：https://www.gnu.org/software/make/manual/make.html


















