# elf 文件格式0 - 可重定位目标文件

在系统中，通常使用  ./a.out 或者  /usr/bin/xxx 之类的命令来运行各种各样的程序，我们只知道，键入命令就会有相应的应用程序被执行，输出想要的结果，不知道你曾经是否有这样的疑问：

* 这些文件为什么可以运行？
* 它又是怎么被运行的？

这并不是简单的两个问题，要弄清楚这两个问题，需要理解 linux 中两个重要的知识点：elf 文件格式，以及程序的加载。 

本系列文章将会对这两个部分进行探究，包括可执行文件、动态库、以及程序的链接加载。




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

由 readelf -h 读取的头部信息已经很清晰明了,下面是头部对应的C语言中结构体:

```
typedef struct {
  unsigned char	e_ident[16];		/* ELF "magic number" */
  unsigned char	e_type[2];		/* Identifies object file type */
  unsigned char	e_machine[2];		/* Specifies required architecture */
  unsigned char	e_version[4];		/* Identifies object file version */
  unsigned char	e_entry[4];		/* Entry point virtual address */
  unsigned char	e_phoff[4];		/* Program header table file offset */
  unsigned char	e_shoff[4];		/* Section header table file offset */
  unsigned char	e_flags[4];		/* Processor-specific flags */
  unsigned char	e_ehsize[2];		/* ELF header size in bytes */
  unsigned char	e_phentsize[2];		/* Program header table entry size */
  unsigned char	e_phnum[2];		/* Program header table entry count */
  unsigned char	e_shentsize[2];		/* Section header table entry size */
  unsigned char	e_shnum[2];		/* Section header table entry count */
  unsigned char	e_shstrndx[2];		/* Section header string table index */
} Elf32_External_Ehdr;
```

elf 头部的数据结构更能够反映出头部的相关信息,各字段,排列顺序等,可以计算得出该结构体总共占用 52 字节,对于 elf64 而言, e_entry,e_phoff,e_shoff 这三个成员为 8 字节,因此多出 12 字节,总共 64 字节.  

下面我们结合 readelf -h 命令给出的信息来详细介绍每个字段.    

### e_ident
e_ident 是一个包含 16 字节的数组成员,对应 readelf -h 给出的 magic 部分.

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


### e_type
type 表示 elf 文件的细分类型,总共有四种:
* 可重定位的目标文件
* 可执行文件
* 动态链接文件
* coredump 文件,这是系统生成的调试文件.  

这四种类型的文件各有各的特点,比如可重定位的目标文件针对的是链接器.  

而可执行文件针对加载器,需要被静态加载到内存中执行,而动态链接文件则是运行过程中的加载.  

coredump 文件主要保存的是系统出错时的运行断点信息,方便人为地或者借助 gdb 分析 bug.  

### e_machine
标识指定的机器,比如 40 代表 ARM.  

其它的比如 x86,mips 等都对应不同的编码.  

### e_version
四个字节的 version code

### e_entry
程序的入口虚拟地址,对于可重定位的目标文件默认是0,而对于可执行文件而言是真实的程序入口.  

### e_phoff
四个字节的 program headers 的起始偏移地址,关于 Program headers 将在后续的章节中详谈.   

### e_shoff
四个字节的 section headers 的起始偏移地址,

### e_flags
和处理器相关的标志位集合,不同的处理器有不同的参数,根据 e_machine 进行解析.  

### e_ehsize
指示 elf header 的 size,对于 arm 而言,52 或者 64.

### e_phentsize
每一个 program header 的 size,在可重定位目标文件中为 0.

### e_phnum
该文件中一共有多少个 program header,在可重定位目标文件中为0.

### e_shentsize
文件中每一个section header 的大小,通常是 40.

### e_shnum
该文件中一共有多少个 section header,上述的示例文件中为 10 个.

### e_shstrndx
在 elf 格式的文件中,符号,section,文件的命名通常是字符串,这些字符串并不会保存在其对应的 section 中,而是统一地使用一个字符串表来保存,该字段指示节标题字符串所在的 section,在上面的示例中,section 标题(.text,.data,...)对应的 e_shstrndx 即段序号为 7,即保存在 .shstrtab 段中.这些 section 标题在链接的过程中需要使用到,在程序执行时是无用的,所以分开有利于精简 section 内容的大小,从而程序加载运行时需要更小的空间.    

除了 section 标题,还有符号命,文件名等字符串,这些默认会被保存在 .strtab section 中.  

## section信息
根据上文中的 elf 文件框架，在 elf 头部之后就是各 sections，section headers 按照一定的格式存放每个 section，包含具体的数据，比如 text 段中包含代码部分，数据、bss 段中包含数据。每种数据都有对应的段解析方式，这一部分我们将在后续的文章中详细解析。  

这一章我们主要研究 elf 目标文件的框架，所以需要获取 section 的描述信息，而 section 的描述信息被保存在 section headers 中，每一个段都由一个 section header 来描述，如果说 elf 头部是整个 elf 文件的"目录"，那么 section headers 就是文件内段信息的"目录"。 

从 elf 头部信息可以看出，program headers 的起始地址为0，表示当前 elf 文件中不存在 program headers，section headers 的起始地址为 264，文件中段的数量为 10，每个段的 size 为 40。  

使用 readelf 命令查看文件内所有的段信息：

```
readelf -S foo.o

There are 10 section headers, starting at offset 0x108:

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .text             PROGBITS        00000000 000034 000010 00  AX  0   0  4
  [ 2] .data             PROGBITS        00000000 000044 000004 00  WA  0   0  4
  [ 3] .bss              NOBITS          00000000 000048 000004 00  WA  0   0  4
  [ 4] .comment          PROGBITS        00000000 000048 000033 01  MS  0   0  1
  [ 5] .note.GNU-stack   PROGBITS        00000000 00007b 000000 00      0   0  1
  [ 6] .ARM.attributes   ARM_ATTRIBUTES  00000000 00007b 000035 00      0   0  1
  [ 7] .shstrtab         STRTAB          00000000 0000b0 000055 00      0   0  1
  [ 8] .symtab           SYMTAB          00000000 000298 0000f0 10      9  11  4
  [ 9] .strtab           STRTAB          00000000 000388 000027 00      0   0  1
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings)
  I (info), L (link order), G (group), T (TLS), E (exclude), x (unknown)
  O (extra OS processing required) o (OS specific), p (processor specific)
```

终端所显示的信息非常详细，该文件一共十个段，其中第一个为 NULL，尽管它在 section 部分不占据任何空间，但是同样对应一个 section header 描述，占用 40 字节，内容为全0。  

每个段的描述信息中都包含了起始偏移地址、size 以及段属性，其中：
section header0 ~ section header7：对应的段内容被保存在 elf 和 section headers 之间的段数据中，比如 text 段内容的起始地址为 0x34，即 52，紧随着 elf 头(elf头 52 字节)，而 .data 段又紧随着 .text 段放置，当然，段之间会设置数据对齐，对齐宽度可以自由设置，默认情况下是 4.   

section header8 和 section header9 比较特殊，它们是 elf 文件的符号表和字符串表，被存放在 section headers 之后，对于可重定位的目标文件而言，这两个表将会在链接阶段被使用。  


在文章开头博主就贴出了整个 elf 文件的布局图，通过 readelf 给出的信息验证一下到底是不是这样：
* 文件头 52 个字节，描述整个文件的属性，包括：类型、段表位置、段表数量及长度等等。  
* 接下来就是段信息，从上图中段表信息可以看到，最开始的 .text 段起始地址为 0x34(52)，所有段依次紧密相连，直到 .shstrtab 段，它的起始地址为 0xb0，长度为 0x55，所以段信息结束位置可以算得： 0xb0 + 0x55 = 0x105，十进制为 261。因此，中间的段信息部分占用地址为 0x34~0x108(对齐到4字节)。
* 从 elf 头部可以看到，section headers 的起始地址为 264(十六进制为0x108)，紧随着第二部分的段信息，而整个 section headers 的长度在 elf 中也有给出，为 10(段) * 40(per size) = 400，所以整个 section headers 占用的地址为 264~664，对应的 16 进制为 0x108 ~ 0x298。
* 再次看上面的段表信息， .symtab 的起始地址正是 0x298，紧随着 section headers 的结尾，.strtab 紧随在 .symtab 之后，.strtab 的结束地址为 0x388+0x27=0x3af。  
* 使用 ls -l 命令查看整个 foo.o 的文件大小，发现文件的大小为 943，正好是 .strtab 段结束的位置。  

到这里，整个可重定位类型的 elf 目标文件的布局已经非常清楚地展现出来。当然，这仅仅是个开始，因为我们还没有详细讨论其中的内容，而这些才是最关键的所在，这些我们将在后文中详细讨论。   




