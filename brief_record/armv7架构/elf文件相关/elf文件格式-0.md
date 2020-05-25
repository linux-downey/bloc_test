# elf 文件格式0 - 目标文件


## elf 文件格式总览
让我们来看看一个目标文件的 elf 格式总体布局是怎样的：TODO


就像一本书总会有目录前言一样，elf 文件在最开始的部分是一个文件头，文件头的作用就是整个文件的描述+目录，使得外部的程序通过读取文件头就可以获取整个文件的大致信息，比如文件类型、版本、对应的机器等。  

文件的第二部分是程序表 program header table，这是可执行文件相关的，我们将在下一节详细讨论这部分。  

第三部分是 sections，也就是我们经常提到的代码段、数据段所在的地方。  

最后一部分为段表，section header table，段表的作用是对文件中所有的段进行描述，段的起始地址，大小等信息。  

## 程序的链接与加载
在分析 elf 文件之前，我们需要了解 elf 文件的类型，elf 文件通常有三种类型：
* 可重定位二进制目标文件
* 可执行文件
* core dump 文件，这类文件是程序出错时产生的调试文件

对于二进制目标文件和可执行文件

## shdr(section header) 和 phdr(program header)

## 读取 elf 文件的软件


## 授人以鱼不如授人以渔
对于底层的分析而言，最好的方式是动手调试，看书往往只能建立一个整体的概念，尤其是在 linux 环境下工作，开源给我们带来了非常大的便利。   

上文中提到，linux 下有不同的命令可以解析 elf 文件并可视化地打印出来，你有没有想过，这些命令是如何做到能显示 elf 内部文件信息的呢？  

答案无非是它们可以读取并解析 elf 文件，那么，我们只需要拿到这些命令的源代码，找到文件的解析部分，elf 文件的所有细节可谓是尽收眼底。   

readelf 和 objdump 的源码位于 binutils 源码包中，[下载地址点这里](https://ftp.gnu.org/gnu/binutils/)，需要注意的是，参考 readelf.c , objdump.c 的实现即可。  

同时，除了看源码，还可以参考[官方文档](https://static.docs.arm.com/ihi0044/g/aaelf32.pdf)，参考官方文档的好处在于可以先建立一个完整的概念，但是需要注意的一点在于，需要特别注意文档的版本。  


## 示例代码
因为我们研究的是 elf 文件的整体框架，所以一个简单的示例就足够了，复杂的代码仅仅是增加了代码段的 size：

foo.c :
```C
int param1;
int param2 = 0;
int param3 = 1;
int main()
{
    return 0;
}
```

在这个示例中，我们定义了三个全局变量，按照惯例，param1 和 param2 会被保存在 bss 段，因为它们没有被初始化或者初始化为 0 ，而 param3 会被保存在数据段，main 函数的实现则是在代码段中。   

将 C 程序编译为可执行文件：

```
gcc -c foo.c
```

生成可执行文件 foo.o ,使用 linux 下的 file 指令查看其文件类型：

```
root@hd:~# file foo.o

foo.o: ELF 32-bit LSB  relocatable, ARM, EABI5 version 1 (SYSV), not stripped

```


## elf 文件头
对于不同的架构或者平台而言，elf 的格式上会有一些小的区别，比如对于 arm32 而言，elf 头是 52 字节，而 arm64 的 elf 头是 64 字节，在本章中我们主要针对 32 位平台进行分析。  

通过 readelf 命令读取可视化的文件头:

```
root@hd:~# readelf -h foo.o

ELF Header:
  Magic:   7f 45 4c 46 01 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF32
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              REL (Relocatable file)
  Machine:                           ARM
  Version:                           0x1
  Entry point address:               0x0
  Start of program headers:          0 (bytes into file)
  Start of section headers:          264 (bytes into file)
  Flags:                             0x5000000, Version5 EABI
  Size of this header:               52 (bytes)
  Size of program headers:           0 (bytes)
  Number of program headers:         0
  Size of section headers:           40 (bytes)
  Number of section headers:         10
  Section header string table index: 7
```

由 readelf -h 读取的头部信息非常清晰,其中的每一个字段都对应解析.  

## magic
magic 部分就是我们所说的魔数,魔数通常就是自定义的识别码,对于 32 位的 elf 文件而言,magic 部分有 16 个字节.  

大部分的文件组织形式都是这样的,头部是一串特殊的识别码,标识该文件的一些概要信息,主要用于外部程序快速地对这个文件进行识别,快速地判断文件类型.  

但是 readelf 命令仅仅是显示了对应的二进制码,并没有进一步显示整个魔数字段的详细信息,关于这一部分就需要参考 readelf 源码来进行分析了,分析结果如下:
* 前四个字节:7f 45 4c 46,识别码, 0x45,0x4c,0x46 三个字节的 ascii 码对应 ELF 字母,通过这四个字节就可以判断文件是不是 elf 文件.  
* 第五个字节:其中 01 表示 32 位 elf 文件,02 表示 64 位.
* 第六个字节:其中 01 表示 小端模式,02 表示 大端模式.
* 第七个字节:表示 EI_version,1 表示 EV_CURRENT,只有 1 才是合理的(代码中是 EI_versoin,但是博主没有进一步具体研究).
* 第八个字节: 00 表示 OS_ABI
* 第九个字节: 00 表示 ABI version 
* 其它字段,源码中没有找到对应的解析,暂定为reserver.

readelf -h 显示的头部 16 字节信息中前七行(包括ABI_version)都被包含在 magic 中.  

## type
type 表示 elf 文件的细分类型,总共有四种:
* 可重定位的目标文件
* 可执行文件
* 动态链接文件
* coredump 文件,这是系统生成的调试文件.  

这四种类型的文件各有各的特点,比如可重定位的目标文件针对的是链接器.  

而可执行文件针对加载器,需要被静态加载到内存中执行,而动态链接文件则是运行过程中的加载.  

coredump 文件主要保存的是系统出错时的运行断点信息,方便人为地或者借助 gdb 分析 bug.  

## e_machine
标识指定的机器,比如 40 代表 ARM.  

其它的比如 x86,mips 等都对应不同的编码.  

## e_version
四个字节的 version code

## e_phoff
四个字节的 program headers 的起始偏移地址,64 位下为八字节,关于 Program headers 将在后续的章节中详谈.   

## e_shoff
四个字节的 section headers 的起始偏移地址,64 位下为八字节,






