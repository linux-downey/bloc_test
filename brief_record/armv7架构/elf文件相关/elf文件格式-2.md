# elf 文件格式 - section
通过前面章节的讨论，对 elf 文件格式以及布局有了一个大致的了解，接下来我们将深入到文件内部，了解整个文件最核心的部分：sections。  

各 section 中包含的数据大部分都是在程序加载阶段被加载进内核参与程序执行的，如果要弄清楚程序是如何加载并执行的，就非常有必要了解各个 section 的组成。    

同样的，关于程序的编译加载，仅仅看书是不够的，必须还需要源代码作为参照，从书中可以快速地获取概要信息，而真正的细节实现还是需要通过分析源代码，甚至通过源代码纠正书籍中存在的版本过时问题。本文中参考的源代码为：binutils-2.34 源码包中的 readelf.c 及相关头文件，readelf.c 也正是命令  readelf 的实现。    


## 从 section header table 开始
在前面的文章中有提到，所有 section 的被保存在 program header table 和  section header table 之间，而 section header table 中保存的是 section 的描述信息，每一个 section 对应 section header table 中一个条目。   

还是以前面章节中那个简单的示例作为讲解：

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

将其编译成可执行文件，这里选择可执行文件而不是目标文件的原因在于可执行文件中包含一个执行程序需要的所有信息，而目标文件只是半成品，不具有代表性。   

通过 readelf 指令获取的头部信息中，我们只关注下列与 section 相关的条目：

```
...
Size of section headers:           40 (bytes)
Number of section headers:         30
...
```

一共有 30 个 section，所以 section header table 中有 30 个描述 section 的 section header，每个 section header 对应 40 个字节，我们首先需要搞清楚的就是：section header 是如何通过这 40 个字节是完整描述一个 section 的？  


## section header
对于 section header 的信息，其实可以从 readelf -S 命令中找到一些蛛丝马迹，当我们使用 readelf -S foo 查看 section header 的时候，可以得到下面的信息：

```
Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .interp           PROGBITS        00008154 000154 000019 00   A  0   0  1
  [ 2] .note.ABI-tag     NOTE            00008170 000170 000020 00   A  0   0  4
  [ 3] .note.gnu.build-i NOTE            00008190 000190 000024 00   A  0   0  4
  ...
```

不难猜到，section header 中保存的内容应该是和第一行的 Name、Type、Addr 等标题相对应。   

是不是这样，通过查看源代码就一目了然，在源码中 section header 对应的结构体如下：

```
typedef struct {
  unsigned char	sh_name[4];		/* Section name, index in string tbl */
  unsigned char	sh_type[4];		/* Type of section */
  unsigned char	sh_flags[4];		/* Miscellaneous section attributes */
  unsigned char	sh_addr[4];		/* Section virtual addr at execution */
  unsigned char	sh_offset[4];		/* Section file offset */
  unsigned char	sh_size[4];		/* Size of section in bytes */
  unsigned char	sh_link[4];		/* Index of another section */
  unsigned char	sh_info[4];		/* Additional section information */
  unsigned char	sh_addralign[4];	/* Section alignment */
  unsigned char	sh_entsize[4];		/* Entry size if section holds table */
} Elf32_External_Shdr;
```

### 成员介绍

整个结构体包含 10 个成员，正好对应 40 个字节，每个 section header 按照顺序依次保存下列的内容：
* sh_name : 对应的 section 名称在 .shstrtab section 中的偏移地址,所有的 section 名称都是被保存在 .shstrtab section 中,因为对于程序的加载运行而言,section name 并没有意义,使用偏移地址的方式保存只会占用固定的 4 字节,而如果直接将 name 放置到这个字段中,一方面长度不固定造成一些编程上的麻烦,另一方面也会造成内存的浪费.  
* sh_type : 该 section 的类型,section 常见的类型如下:

  * SHT_NULL(0):           对应 elf 文件中的第一个 NULL section.
  * SHT_PROGBITS(1):       程序数据,也就是二进制的机器码.
  * SHT_SYMTAB(2):         符号表,链接时需要用到.
  * SHT_STRTAB(3):         字符串表.
  * SHT_RELA(4):           重定位偏移信息.
  * SHT_HASH(5):           符号对应的 hash 表,用于快速查找.
  * SHT_DYNAMIC(6):        动态链接相关信息
  * SHT_NOTE(7):           用于标记文件的信息
  * SHT_NOBITS(8):         该 section 在 elf 文件中不占用空间   
  * SHT_REL(9):            重定位信息
  * SHT_DYNSYM(11):        动态链接符号表
  * SHT_INIT_ARRAY(14):    函数指针数组,数组中的每个函数指针执行初始化工作,在 main 之前执行.  
  * SHT_FINI_ARRAY(15):    函数指针数组,数组中的每个函数指针执行收尾工作,在 main 之后执行.  
  * SHT_PREINIT_ARRAY(16): 函数指针数组,数组中的每个函数指针执行更早期的初始化工作,在 main 之前执行.  
  * SHT_GROUP(17):         包含一个 section 组,对于存在一些内部关系的 section,将其归为一个 section group,比如当两个 section 存在内部引用时,一个 section 被删除,而另一个 section 就失去了意义,使用 group 可以绑定 section 之间的关系,但是这种做法并不多见.  

* sh_flags : 该 section 对应的属性,常见的属性有以下几种:
  * SHF_WRITE(1 << 0):                可写的数据
  * SHF_ALLOC(1 << 1):                在执行的时候需要从系统申请内存
  * SHF_EXECINSTR(1 << 2):            可执行的指令数据
  * SHF_MERGE(1 << 4):                该 section 中的数据可以被合并
  * SHF_STRINGS	(1 << 5):             由 '\0' 结尾的字符串数据
  * SHF_INFO_LINK	(1 << 6):           保存 section header 的索引
  * SHF_LINK_ORDER	(1 << 7):         在链接时保留节顺序.
  * SHF_OS_NONCONFORMING (1 << 8):    该 section 需要特定于操作系统的处理 
  * SHF_GROUP	(1 << 9):               section group 中的成员
  * SHF_TLS (1 << 10):                线程本地存储数据 section
  * SHF_COMPRESSED	(1 << 11):        压缩数据

* sh_addr:该 section 对应的执行时虚拟地址,对于目标文件而言,所有 section 都为 0,因为目标文件中的 section 无法确定最后的执行地址,而可执行文件中将要被加载到内存中的 section 将会在链接阶段分配虚拟地址,而那些执行时不被加载的 section ,该值也是 0,典型的不加载的 section 有:.strtab,.symtab,.comment 等.  
* sh_offset:该 section 在文件中的偏移值,需要注意的是,文件中的偏移地址和 section 的虚拟地址并没有线性的偏移关系.  
* sh_size: section 的 size,以 byte 为单位.  
* sh_link: 另一个相关联段的索引地址
* sh_info: 额外的 section 信息.  
* sh_addralign: section 内的对齐宽度.注意区分section 内对齐宽度与 section 之间的对齐宽度,section 之间的对齐宽度由下一个 section 的 sh_addralign 来决定,比如当前 section 对齐宽度为 4,下一个 section 的对齐宽度为 1,而当前 section 最后一个条目结束地址为 103 bytes,下一个 section 的起始地址就是 103,而不会对齐到 104.  
* sh_entsize: 如果该 section 中保存的是列表,该字段指定条目大小.   

## 不同类型的 section 数据组织方式
上文中提到不同的 section 可能分属不同的类型,而不同的类型自然包含着不同含义的数据,起到不同的作用,有一部分 section 针对链接过程而存在,有一部分针对加载过程而存在.针对加载过程的 section 数据分为两种:一种是需要被加载器进行解析,给加载过程提供辅助信息,另一种就是程序代码和数据,加载器直接将其 copy 到对应的虚拟地址即可,二进制代码和数据由 CPU 执行.   

对于需要解析的 section 数据,自然需要规定在它组成和解析的时候遵循特定的格式,这样链接器或者加载器才能按照格式对它进行解析,从程序中的角度来看这就涉及到不同的数据结构.   

接下来就来分析各个 section 对应的数据结构:

### SHT_SYMTAB
对应 SHT_SYMTAB 类型的 section 中保存的数据是程序的静态符号表,静态符号表是对应链接过程,符号表中每个符号所对应的数据结构为:

```
typedef struct {
  unsigned char	st_name[4];		/* Symbol name, index in string tbl */
  unsigned char	st_value[4];		/* Value of the symbol */
  unsigned char	st_size[4];		/* Associated symbol size */
  unsigned char	st_info[1];		/* Type and binding attributes */
  unsigned char	st_other[1];		/* No defined meaning, 0 */
  unsigned char	st_shndx[2];		/* Associated section index */
} Elf32_External_Sym;
```
一个 Elf32_External_Sym 描述一个符号,占 16 个字节:
* byte0~byte3:每个符号都有对应的 name,比如函数名,变量名,该名称为字符串,前文中有提到,字符串是被统一存放在 strtab 中的,该字段的值表示当前符号名在 strtab 中的位置.  
* byte4~byte7:符号的值,注意这里符号的值指的是地址值,比如对于变量 val = 1,这里符号的值不是 1,而是 val 的地址值.
* byte8~byte11:符号的 size,不同的符号对应不同的 size,比如 char 和 int.该字段记录了当前符号的size,同时需要注意的是,对于函数而言,符号的 size 等于整个函数所占的空间.  
* byte12: 类型以及绑定的属性,该字节分为高四位和低四位,高四位用于描述绑定信息,低四位表示类型,常见的符号的类型有以下几种:
  * STT_NOTYPE(0):      未指定类型.
  * STT_OBJECT(1):      该符号是一个数据对象
  * STT_FUNC(2):        该符号是一个代码对象,虽然名称为 FUNC,但是它也可以是汇编代码中的标号,比如 _start,所以用代码对象描述更合适
  * STT_SECTION(3):     与 section 相关联的符号
  * STT_FILE(4):        该符号是文件名
  * STT_COMMON(5):      通常是 weak 类型的符号(比如目标文件中未初始化的全局变量)
  * STT_TLS(6):         线程的本地数据
  * STT_RELC(8):        复杂的重定位表达式
  * 其它一些处理器或者 OS 特定的符号类型
  
  常见的绑定状态有以下几种:
  * STB_LOCAL(0):       内部符号,外部不可见,最典型的就是由 static 修饰的静态变量
  * STB_GLOBAL(1):      全局符号
  * STB_WEAK(2):        和全局变量一样,优先级较低
  * 其它一些处理器或者 OS 特定的绑定状态

* byte13: 无意义
* byte14~15:相关联的 section 索引,比如 main 函数对应的 section 为 .text,所以该字段就是 .text 的索引.  

看完这个针对符号的数据结构,是不是感觉少了什么东西?细心的朋友不禁要问:对于变量符号而言,变量的值怎么没有保存在符号表中?   

是的,你没看错,对于变量的值,确实没有保存在符号表中,而是单独保存在 .data section 中.   

为什么将一个变量的描述信息保存在符号表中,而将变量的值放在 .data section 中?要搞清楚这个问题,你得先搞清楚一个问题:程序代码是如何引用数据的?   

在 C 语言中,编译器为每个全局(静态)变量分配一个地址,地址上保存着该变量的值,编译后的程序指令对于这个全局变量的引用实际上完全不需要通过符号表,而是直接将变量的地址硬编码到指令中,所以程序的执行完全不需要符号表的介入,也就是为什么在可执行文件中可以丢弃静态符号表.  

那么为什么链接过程需要符号表呢?这也是因为刚开始变量的地址还没有确定,链接过程需要通过符号表获取变量的信息来进行重定位,重定位过程就是将变量最终的地址硬编码进指令中的过程,重定位一旦完成,静态符号表也就不需要了.    

如果想要了解程序指令的执行以及静态链接过程,可以参考我的另一篇博客:TODO.  

### SHT_NOTE  
SHT_NOTE 类型的 section 用于标记文件的相关信息，属于在加载过程中起到辅助作用的 section，它的数据结构如下：

```
typedef struct {
  unsigned char	namesz[4];		/* Size of entry's owner string */
  unsigned char	descsz[4];		/* Size of the note descriptor */
  unsigned char	type[4];		/* Interpretation of the descriptor */
  char		name[1];		/* Start of the name+desc data */
} Elf_External_Note;
```
Note 类型的数据是不定长的，从这个结构体看来，它的长度应该是 13，但是实际上，char name[1] 是 GNUC 中一种技巧，定义一个长度为 1 或者为 0 的数据，将需要存放的数据都放在 name 起始的位置，同时在给该结构体申请内存时考虑到需要多出来的空间即可。这样的好处在于可以将不确定长度的数据保存在连续的地址内，同时又不浪费空间，这种零长数组的机制在内核中非常常见。  

由此也可以看出，SHT_NOTE 类型的 section 的格式并不是固定的。  

整个 section 的布局是这样的：
* namesz：指定 name 的长度
* descsz：指定 desc 即描述部分的长度。
* type：  desc 部分数据的类型，即如何解析它。
* name：  从 name 开始的地址，就是存放 name + desc 的真实数据部分。  

### SHT_REL
SHT_REL 类型的 section 主要用于链接时的重定位，重定位 section 分两种，一种是链接过程的重定位，而另一种属于运行过程的重定位，这里我们主要讨论链接过程的重定位。   

具体的静态链接过程可以参考我的另一篇博客TODO。  

所以重定位主要是针对代码对数据的引用，对于没有引用全局数据的纯代码而言，放到任何地址都是可以运行的，但是一旦涉及到全局变量，情况就不一样了，因为在链接之前，全局数据是不能确定最终运行地址的，所以在原来的代码指令中对于全局变量的编址需要修改，而修改就涉及到三部分：

* 需要修改的指定的地址
* 需要修改的变量的信息
* 如何修改

重定位 section 所提供的信息必然需要解决这三个问题，看看它的数据结构：

```
typedef struct {
  unsigned char r_offset[4];	/* Location at which to apply the action */
  unsigned char	r_info[4];	/* index and type of relocation */
} Elf32_External_Rel;
```

每个重定位项占 8 个字节，前四个字节表示重定位的位置，这个位置是指在 .text 段中的偏移地址。  

第二个四字节表示重定位的信息：index 以及类型。具体的对应关系如下：
* 高 24 位用于标识需要重定位的符号在符号表中的偏移地址
* 最低 8 位用于标识重定位类型，重定位的类型和具体指令强相关，比如 R_ARM_ABS16 标识需要重定位的指令是 16 位立即数寻址，R_ARM_THM_CALL 标识重定位指令为 thumb 指令等等，这些涉及到具体指令相关的就不过多赘述了。  



```
typedef struct {
  unsigned char r_offset[4];	/* Location at which to apply the action */
  unsigned char	r_info[4];	/* index and type of relocation */
} Elf32_External_Rel;
```

### 无需解析的 section
elf 文件中不需要被解析的 section 主要有以下几个:
* SHT_PROGBITS:二进制代码或者数据,由加载器直接 copy,无需解析,所以这一类 section 中保存的就是纯数据.典型的 section 为 .text,.data 等. 
* SHT_NOBITS:该类的 section 在 elf 文件中不占用空间,由加载器加载时向系统申请相应 size 的内存,而这些 size 的信息保存在 section header 中而不是 section 的内容中,不存在数据自然就不需要解析,典型的 section 为 .bss .
* SHT_INIT_ARRAY,SHT_FINI_ARRAY,SHT_PREINIT_ARRAY:这些 section 中保存的都是函数指针,在同一个平台中,函数指针的长度是固定的(尽管对于某些特殊的平台可能不一样,但是可以规定使用统一长度的指针),因此不需要规定额外的数据组织方式.  

类型为 SHT_PROGBITS 的 section 中保存的基本都是程序代码和数据,这些 section 在加载的过程中并不需要


### 文件分析

来看一下实际的情况是不是这样:我们以第一个 section .interp 为例进行分析,

.interp 是当前文件中第一个有效 section，第一个 section 为 NULL，对应的 section header 为 section header 起始地址+ size of section headers 

在 elf header 对于 section headers table 的描述是这样的:

```
...
Start of section headers:          4508 (bytes into file)
Size of section headers:           40 (bytes)
Number of section headers:         30
...
```
所以, .interp section 对应 section header 在 section header table 中的偏移地址为 4508 + 40 = 4548,占据 40 字节，所以，我们可以使用 hexdump 指令将这 40 字节打印出来：

```
hexdump -C -s 4548 -n 40 foo

000011c4  1b 00 00 00 01 00 00 00  02 00 00 00 54 81 00 00  |............T...|
000011d4  54 01 00 00 19 00 00 00  00 00 00 00 00 00 00 00  |T...............|
000011e4  01 00 00 00 00 00 00 00                           |........|
000011ec

```

接下来就是进行对比，每个 section header 中包含 10 个条目，每个条目占用 4 字节进行描述，因为当前 elf 文件是小端，所以所有的字节都以小端模式进行解析，TODO section header解析：TODO



了解 section header 的描述信息对于后续 section 的解析是非常必要的，同时，在后续的开发中可能也会涉及到要手动地向 elf 文件中添加内容以达到向程序添加功能的目的。  


## 各 sections 描述
接下来我们要来看一看各个 section 中有哪些具体的内容，为了方便对照，先使用 readelf -S foo 命令将所有 section 显示出来：

```
Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .interp           PROGBITS        00008154 000154 000019 00   A  0   0  1
  [ 2] .note.ABI-tag     NOTE            00008170 000170 000020 00   A  0   0  4
  [ 3] .note.gnu.build-i NOTE            00008190 000190 000024 00   A  0   0  4
  [ 4] .gnu.hash         GNU_HASH        000081b4 0001b4 000024 04   A  5   0  4
  [ 5] .dynsym           DYNSYM          000081d8 0001d8 000040 10   A  6   1  4
  [ 6] .dynstr           STRTAB          00008218 000218 00003c 00   A  0   0  1
  [ 7] .gnu.version      VERSYM          00008254 000254 000008 02   A  5   0  2
  [ 8] .gnu.version_r    VERNEED         0000825c 00025c 000020 00   A  6   1  4
  [ 9] .rel.dyn          REL             0000827c 00027c 000008 08   A  5   0  4
  [10] .rel.plt          REL             00008284 000284 000018 08   A  5  12  4
  [11] .init             PROGBITS        0000829c 00029c 00000c 00  AX  0   0  4
  [12] .plt              PROGBITS        000082a8 0002a8 000038 04  AX  0   0  4
  [13] .text             PROGBITS        000082e0 0002e0 000130 00  AX  0   0  4
  [14] .fini             PROGBITS        00008410 000410 000008 00  AX  0   0  4
  [15] .rodata           PROGBITS        00008418 000418 000004 04  AM  0   0  4
  [16] .ARM.exidx        ARM_EXIDX       0000841c 00041c 000008 00  AL 13   0  4
  [17] .eh_frame         PROGBITS        00008424 000424 000004 00   A  0   0  4
  [18] .init_array       INIT_ARRAY      00010f0c 000f0c 000004 00  WA  0   0  4
  [19] .fini_array       FINI_ARRAY      00010f10 000f10 000004 00  WA  0   0  4
  [20] .jcr              PROGBITS        00010f14 000f14 000004 00  WA  0   0  4
  [21] .dynamic          DYNAMIC         00010f18 000f18 0000e8 08  WA  6   0  4
  [22] .got              PROGBITS        00011000 001000 00001c 04  WA  0   0  4
  [23] .data             PROGBITS        0001101c 00101c 00000c 00  WA  0   0  4
  [24] .bss              NOBITS          00011028 001028 00000c 00  WA  0   0  4
  [25] .comment          PROGBITS        00000000 001028 000032 01  MS  0   0  1
  [26] .ARM.attributes   ARM_ATTRIBUTES  00000000 00105a 000035 00      0   0  1
  [27] .shstrtab         STRTAB          00000000 00108f 00010a 00      0   0  1
  [28] .symtab           SYMTAB          00000000 00164c 0006b0 10     29  80  4
  [29] .strtab           STRTAB          00000000 001cfc 000371 00      0   0  1
```

仅仅是编译了一个非常简单的例程，连最基本的 C 库函数 printf 都没有调用，为什么会出现这么多的段？  

实际上,一个程序的编译运行过程远远不是你想象得那么简单，一个程序并不是以 main 开始，也不是以 main 的返回为结尾，在一个进程开始之初,需要运行环境进行一些必要的初始化,比如堆栈空间的初始化,bss 数据的初始化,进程参数的传递等,然后再执行 main 函数.  

这就是为什么你可以直接在 main 函数中申请释放内存,获取命令行传入的参数,这都是由 Glibc 运行库做的,在 main 函数返回时,还会执行一些收尾工作.所以一个基本的程序通常都会链接 C 标准库,主要是初始化和收尾工作,自然就包含额外的一些 section.   

下面我们来看看常见的 section 内容。  

### .interp
包含了动态链接器在文件系统中的路径，通常是 /lib/ld-linux-armx.so.x，在程序加载时如果当前程序依赖于动态库，需要确保其依赖的动态库已经被加载到内存中，而动态库的加载就是由动态链接器完成。所以在程序中需要指定动态链接器在文件系统中的位置。    


### .note.ABI-tag
每个可执行文件都应包含一个名为.note.ABI-tag的SHT_NOTE类型的节。  

本部分的结构为ELF规范中记录的注释部分。该部分必须至少包含以下条目：前面的部分为类型、名称，名称字段包含字符串“ GNU”。类型字段应为1。后续是 desc 字段，应至少为16，desc字段的前16个字节应如下：desc字段的第一个32位字必须为0（这表示Linux可执行文件）。desc字段的第二，第三和第四32位字包含最早的兼容内核版本。例如，如果3个字分别是2、6和2，则表示2.6.2内核。  

该 section 和 text 一样将会被放在 Read/Exec 权限内存内。  

### .note.gnu.build-i
同样是 SHT_NOTE 类型，用于描述该 elf 文件的相关属性，其中保存的是 GNU build ID，

### .gnu.hash 
符号的 hash 表，主要用于符号的快速查找

### .dynsym
动态链接时的符号表，主要用于保存动态链接时的符号

### .dynstr
动态链接时的字符串表，主要是动态链接符号的符号名

### .gnu.version，.gnu.version_r
版本相关的内容

### .rel.dyn ， .rel.plt
动态链接相关的重定位信息，关于动态链接，将在后续的文章详细讨论。  

###  .init ， .fini
.init section 的类型为 INIT_ARRAY，其中包含了





