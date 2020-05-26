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

整个结构体包含 10 个成员，正好对应 40 个字节，每个 section header 按照顺序依次保存下列的内容：
* sh_name :





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

答案是：一个程序的编译运行过程远远不是你想象得那么简单，一个程序并不是以 main 开始，也不是以 main 的返回为结尾，TODO。

下面我们来看看常见的 section 内容。  

### .interp
包含了动态链接器在文件系统中的路径

### .note.ABI-tag
用于指定程序的 ABI 

### .note.gnu.build-i

### .gnu.hash 
符号的 hash 表，主要用于符号的快速查找

### .dynsym
动态链接时的符号表，主要用于保存动态链接时的符号

### .dynstr
动态链接时的字符串表，主要是动态链接符号的符号名

### .gnu.version，.gnu.version_r
版本相关

### 
