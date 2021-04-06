# armv7-A系列8 - GNU伪汇编指令

汇编指令分为两种：硬件汇编指令、伪汇编指令。  

硬件汇编指令非常好理解，就是厂商定义的，针对处理器硬件操作的指令，比如 add、sub 等，但是在实际的汇编程序中，仅仅用硬件汇编指令是无法完成编程工作的。  

一方面，汇编指令经过汇编器被编译成机器码，但是一个完整的程序通常不会仅仅只有指令，当编程者要在二进制文件中插入数据时，硬件汇编指令无法完成这个工作，同时，程序中免不了有很多对于地址或者数据的引用，如果每次引用数据或地址都以硬编码的形式来指定，不仅编程不方便，还会给后面的程序链接过程带来非常大的麻烦。   

所以实际上，因为各种各样的原因，汇编程序中有很多硬件汇编指令不方便做或者压根做不了的事，那么，这些工作就需要伪汇编指令来完成，硬件汇编指令针对处理器具体的操作，而伪汇编指令则是针对汇编器，告诉汇编器要做的事。   

比如下面的汇编代码：

```
main：
    bl foo

foo：
    bx lr
```

其中 main 和 foo 都是伪汇编中的标号，标号不占用实际地址空间，在编译时它的值等于紧随标号后的第一条指令的地址，在上面的汇编程序中，通过标号可以实现跳转，如果不使用标号就需要使用指令相对偏移地址，这种地址硬编码无疑是非常麻烦的。  




## 常见伪汇编指令

伪汇编指令都是大小写不敏感的.

### .align
align 用作将当前地址对齐某个指定的边界，对应的表达式为：

```
align [abs-expr[， abs-expr[， abs-expr]]]
```

第一个参数表示需要对齐的边界，如果不指定，默认值为 0，表示没有对齐要求.需要注意的是不同平台对于参数有不同的解析:有些平台直接使用后面的参数作为对齐的地址，而有些平台将 2 的参数次幂作为对齐地址， arm 使用后一种，比如 .align 3 表示 8 字节对齐.  

第二个参数可选，表示对齐时要插入的字节，可以自由地插入任何想要的字符. 比如 .align 3，0x55， 表示将 0x55 插入到对齐的间隙中.  

第三个参数可选，表示对齐时最大的对齐间隙，比如 .align 4，0，2 ，尽管对齐是按照 16 字节，但是最大的对齐间隙为 4 字节，实际上最大的对齐之后的地址为 当前地址+4. 在 arm 平台上，这个参数的功能并没有被实现.  

### .ascii ， .asciiz

该伪汇编指令对应的表达式为:

```
.ascii string
.asciiz string
```
string 是字符串，也可以是字符串列表，asciiz 相对与 ascii 的区别是，asciiz 会在字符串最后添加一个 \0 结尾，z 表示 zero. 在 arm 编译器的测试中，不支持 .asciiz，同时，字符串的放置不会自动设置对齐，比如 .ascii "hello" ，其存放方式是 .word "hell" .byte 'o'


### .comm
.comm 表示目标文件中的 common symbol，表示公共的符号，它的表达式是这样的:

```
.comm symbol，length
```
这和 GNU 中的强弱符号机制相关，未初始化的变量表示为弱符号，初始化的变量为强符号，当不同源文件中存在多个同名变量时，强符号会覆盖弱符号而不会报错，这是 gcc 的扩展语法，所以实际上未初始化的全局变量是作为公共符号保存的，当多个文件中的 comm 符号出现冲突时，需要将其以一定规则融合.   

实际上，C 语言中未定义的全局变量(也就是 comm 符号)并非是存放到 bss 段中的，而是保存在 COMMON 段.  

### .byte ， .word ， ...
伪汇编中有一系列的数据放置指令，表示在当前位置放置某些数据，相对应的有:

.byte : 放置一个字节
.hword:放置半字，在 32 位平台中对应两个字节，64 位对应四字节
.short:放置一个 short 类型数据，两个字节
.word:放置一个字，在 32 位平台中对应四个字节，64 位对应八字节
.int : 放置一个 int 类型的数据，数据长度根据平台而定，16位平台为两字节，32位和64位平台为四字节
.long:放置一个 long 类型的数据，数据长度根据平台而定，32位平台为四字节，64位平台为八字节
.float:放置一个 float 类型数据，四字节
.double:放置一个 double 类型数据，八字节

### .if ， .elseif ， .else ，.endif
条件表达式， .if 接一个逻辑表达式，逻辑表达式的结果为 false 或 true， .if 有多种变种的形式，比如:
.ifdef:判断是否定义
.ifeq:判断相等
.ifge:判断大于等于
...

.else 表示分支，
在条件判断的最后必须以 .endif 结尾.

在 armv7 平台上，基本上看不到伪汇编条件表达式的影子，因为 armv7 指令集中大部分指令都自带条件执行的特性.  

### .globol， .globl， .extern ， .local
符号作用域标识符， 

.globol， .globl 两者的效果相同，都是定义全局符号，符号对 ld 可见. 

.local 标示一个符号为本地，对外不可见，如果该符号未定义，就定义该符号. 

.extern，标示一个符号为外部导入的.这个符号只是为了兼容其它的汇编器，事实上，所有的未定义符号都会被视为外部导入.  

### .macro ， .endm ，.exitm

定义一个宏包含一段代码，宏可以接受参数，且必须以 .endm 结尾，也可以使用 .exitm 提前退出该宏的定义.宏的概念和Ｃ语言中是一致的，宏只进行替换.    

不带参的宏定义:
```
.macro foo
    ...
.endm
```

带参的宏定义:
```
.macro foo arg1，arg2
    ...
.endm
```

宏的调用方式为直接使用宏名，有参数时多个参数以逗号进行分隔，宏的使用必须是先定义再使用，它跟标号的区别在于，标号处的指令会被编译进目标文件.而宏只有在被调用的时候才会将其中的指令替换到对应的地址.    

### .section
定义一个段，对于不同的文件格式有不同的形式， linux 中主要使用 elf 文件格式，对于 elf 格式而言， .section 的通用表达式为:

```
.section name [，"flags"[，@type[，flag_specific_arguments]]]
```

除了 name，其它部分都是可选的，比如下面的定义:

```
.section .foo
    text...
.section .bar
    text...
```
将会定义两个段 .foo 和 .bar.  

同时，还可以指定该段的属性，对应的属性见下表:

```
a section is allocatable
d section is a GNU_MBIND section
e section is excluded from executable and shared library.
w section is writable
x section is executable
M section is mergeable
S section contains zero terminated strings
G section is a member of a section group
T section is used for thread-local-storage
```
多种属性可以组合， 比如:

```
.section .foo，"aex"
    text...
```
这些段属性的设置将会体现在段信息中.使用 objdump -h foo.o 时可以查看到对应的段属性设置.  

d 属性在 arm 编译器中没有实现.对于某些属性比如 M 还有对应的参数，这些特殊的用法比较少见，有兴趣的可以参考[官方文档](https://sourceware.org/binutils/docs/as/Section.html#Section)，不过建议在参考的同时进行相应的测试，因为 arm 编译器的实现可能会有差异的地方.

### .set， .eqv， .equ， .equiv
.set:设置一个符号的值，这种定义在形式上类似于 C 语言中的变量，和 C 中的变量不一样的是，它只作用于编译阶段，而不会对应运行时地址空间，因此更像是 C 中的宏。通过设置一个符号的值，在后续的代码中可以重复使用该符号的值。我们可以看下面的示例：

```
.set sym_set，0x100
mov r0，#sym_set
```
执行结果就是 r0 中保存了 0x100.   

在 GNU 的定义中，.set ， .eqv， .equ 和 .equiv 所起的作用是类似的.对于是否带有修改变量的功能，不同的编译器可能有不同的实现.  

在 armv7 的 4.20 版本编译器实现中， .set， .eqv 和 .equiv 都不能对符号值进行修改，只能定义. 而 .equ 可以修改.



### .text ，.data 
分别对应指定的段，表示标号之后的指令或者数据被存放在指定的段中，直到遇到下一个段标识符.  


### .print， .error， .warning
打印调试信息，针对编译器的编译阶段.这几种指令的使用都是添加参数 string，比如:

.print "foo"
.error "bar"
.warning "hello"

区别在于，.error 会中断编译过程， .warning 表示给出警告，但是并不终止编译， .print 仅仅是打印信息. 

### .weak
定义一个 weak 类型的符号，当这个符号在相同作用域的地方存在定义，当前符号会被忽略，如果这个符号之前不存在，这个符号就会被使用. 这和 C 语言中的 weak 机制是一样的.  

### .type
设置一个符号的属性，对于 elf 格式的文件，该伪汇编指令的表达式为:

```
.type symbol_name， %type
```
表达式支持的格式为:

```
.type <name> STT_<TYPE_IN_UPPER_CASE>
.type <name>，#<type>
.type <name>，@<type>
.type <name>，%<type>
.type <name>，"<type>"
```

对于 armv7 编译器而言，不支持 @ 符号的格式，即第三种.  

其中，type 对应的类型为:

```
STT_FUNC
function
    Mark the symbol as being a function name.

STT_GNU_IFUNC
gnu_indirect_function
    Mark the symbol as an indirect function when evaluated during reloc processing. (This is only supported on assemblers targeting GNU systems).

STT_OBJECT
object
    Mark the symbol as being a data object.

STT_TLS
tls_object
    Mark the symbol as being a thread-local data object.

STT_COMMON
common
    Mark the symbol as being a common data object.

STT_NOTYPE
notype
    Does not mark the symbol in any way. It is supported just for completeness.
```

### .size
设置一个符号的 size，表达式为:

```
.size name，nbytes
```
一个比较常用的技巧是使用伪汇编中的特殊符号 "."， "." 可以代表当前地址，所以设置一个函数的 size 可以这样写:

```
.type foo，%function
foo:
    text...
    .size foo， .-foo
```

. - foo  表示当前地址减去函数基地址.  



### .ident
放置一些标识符到目标文件中，这一部分属于注释类，被放在 .comment 段中.  


## arm 平台相关伪指令
上文中所提到的都是 GNU 的标准，针对特定的平台，还会有一些特殊的处理指令或者是基于 GNU 标准上的修改.对于 arm 平台而言，有以下的特点:



* 可以通过 .syntax 指令选择不同的语法，如果不指定默认使用 divided 旧样式语法，可以通过 .syntax unified 选择新的统一语法，这种新的统一语法被 arm 编译器使用.  
* 伪汇编指令的立即数不需要添加 # 前缀，硬件汇编指令依旧需要，属于新语法的特性.
* 特殊字符 @ 总是会被用来注释解析，所以 GNU 伪指令中出现的以 @ 为特殊字符的指令都无法使用.  
* 对于 arm 的重定位代码的生成，允许使用 #:lower16:foo 和 #:upper16:foo 来引用一个 .word 类型 foo 的高 16 位和低 16 位，而且这种做法是非常通用的.对于一个全局的符号(全局变量)，在进行链接之前是不知道它真实运行时的地址的，也就无法通过地址对全局符号进行引用，所以需要通过伪汇编来对符号进行引用，在 armv7 指令中，movw 和 movt 可以加载任意的 16 位立即数，而几乎所有的其它指令都只能加载有规则限制的立即数，所以，对于 .word 数据中高(低) 16 位数据进行访问的支持是非常必要的.    

### .arch
选择特定的 arch 平台，这个参数和命令行的 -march 同样的作用，对应的参数可以是指令集架构型号或者扩展型号比如:armv7-a.  

### .arm， .thumb， .thumb_func， .code
这些指令设置指定的指令集:
.arm :指定接下来的指令被编译成 arm 指令集
.thumb:指定接下来的指令被编译成 thumb 指令集
.code 16|32: .code 16 同 .thumb， .code 32 同 .arm，虽然现在的 thumb 指令集也有 32 位的，因为历史原因依旧使用 16 位来描述 thumb 指令集.  

### .bss
和 .text， .data 一样，表示接下来的数据部分将会放在 .bss 段，需要注意的是， .bss 并没有在标准的伪汇编指令中出现，而在平台相关的伪汇编指令定义.  


### NOP，LDR， ADR，ADRL
这四个伪汇编指令是由 arm 硬件汇编扩展出来的:

NOP: NOP 指令等于 mov r0，r0

LDR: LDR 同时是硬件汇编指令的一个，作用是从指定的地址，对于伪汇编而言，它的使用有一些不同:

```
ldr Rn， = expr
```
Rn 表示指定的寄存器，expr 可以是表达式，可以是立即数或者某个标号.如果是立即数，因为是伪汇编指令，不需要添加 # 前缀. 

ADR:ADR 用于获取某个标号的地址，放置到指定的寄存器中，这个指令将会被汇编器解释成一条针对 pc 寄存器的 sub 或者 add 指令.比如:

```assembly
adr r0， = foo
...
foo:
    text...
```

将会汇编器翻译成这样一条指令:

```assembly
add     r0， pc， #n
```



### 参考

[GNU汇编官方资料:Using as](https://sourceware.org/binutils/docs/as/)

[armv7-A-R 参考手册](https://gitee.com/linux-downey/bloc_test/blob/master/%E6%96%87%E6%A1%A3%E8%B5%84%E6%96%99/armv7-A-R%E6%89%8B%E5%86%8C.pdf)



[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。