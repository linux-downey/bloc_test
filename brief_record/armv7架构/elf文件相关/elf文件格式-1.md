# elf 文件格式-1- 可执行文件

因为考虑到可执行文件和可重定位文件之间有许多差异，所以在上一章中，仅仅是讨论了 elf 中可重定位目标文件的布局，这一章我们再来详细讨论 elf 文件中可执行文件的布局.  




## 可重定位目标文件和可执行文件的区别
可重定位目标文件是编译过程产生的中间产物，通过程序的链接最终产生可执行文件，而我们一般运行的程序就是可执行文件.  

如果不去关注编译的细节，我们可以将编译简单地分为以下几个部分:



* 预编译:主要是宏的处理和头文件包含处理.
* 编译:将单独的 c 文件编译，生成对应的汇编代码
* 汇编:处理汇编代码和伪汇编代码，生成机器码，这一步将生成可重定位目标文件
* 链接:将可重定位文件进行链接，生成最终的可执行文件.  

使可执行文件和可重定位目标文件产生差异的地方大部分源自于 gcc 的分离编译模式，也就是单独地编译每个 .c 文件，生成独立的 .o 可重定位目标文件，因此，可重定位文件和可执行文件有以下区别:



* 链接过程需要进行符号解析，所以可重定位目标文件中存在大量的符号以及重定位信息，这些符号主要辅助完成链接阶段，可执行文件不需要这些信息，但是默认会保留。
* 可执行文件可以在指定的地址上执行，所以它需要提供信息给加载器，而可重定位目标文件只需要提供信息给链接器，反映在 elf 文件中就是可执行文件中存在 program header，而可重定位目标文件中没有.  

关于程序的静态链接过程可以参考[程序的静态链接](https://zhuanlan.zhihu.com/p/363010286).



## section 和 segment
在上一章中我们讨论到，可重定位目标文件中有多个 section，而且整篇文章我都是使用 section 来描述而不是把它翻译成"段"或者"节".  

这是因为在可执行文件中，存在另一个概念:"segment"，因为这两个单词都有"段"和"节"的含义，所以为了避免混淆，干脆就不翻译了.  

section 针对的是链接器，而 segment 是可执行文件中的概念，针对的是加载器.   

对于程序的加载而言，最终都是加载到内存中，如果按照 section 的方式加载，通常的做法就是将不同属性的内容分页加载，一方面过多的 section 将造成内存的浪费，另一方面，操作系统完全不关心数据属于哪个 section.对于内存访问而言，操作系统更加关心的是内存的操作权限，比如 .text 段权限为读/执行权限，.data 段的读写权限.   

所以，可执行文件将多个可重定位目标文件中的 section 根据操作权限分为多个部分，将多个操作权限相同的 section 进行组合(并不是只要权限相同就一定组合到一起)，然后映射到对应的内存地址，于是多个 section 组成一个 segment，在同一个 elf 可执行文件中存在多个 segment，就像 section headers 负责记录所有 section 的描述信息，对应的多个 segment 的描述信息保存在 Program headers 部分，程序加载时通过读取 Program headers，或者各个 segment 并加载.    



## elf 可执行文件布局
对于 elf 可执行文件，布局如下:

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/elf/elf%E5%8F%AF%E6%89%A7%E8%A1%8C%E6%96%87%E4%BB%B6%E5%B8%83%E5%B1%80.jpg) 

由图可知，相对于可重定位目标文件而言，可执行文件多了一个 program headers 部分，同时，最后的符号表部分不再是必需项了，因为在可重定位目标文件中，符号表和字符串表是给链接器工作时提供辅助信息的，而加载阶段并不需要，但是它默认会保存在可执行文件中以提供一些调试时的辅助信息，如果需要削减可执行文件大小，可以使用 strip 指令将其去除.  



## elf 文件分析
接下来我们来逐步地验证 elf 可执行文件的布局.  

还是使用上一章那个简单的 C 文件:

```
int param1;
int param2 = 0;
int param3 = 1;

int main()
{
    return 0;
}

```
然后编译:

```
gcc foo.c -o foo
```

生成可执行文件 foo. 




### 文件头
同样使用 readelf 读取文件头:

```
readelf -h foo

ELF Header:
  Magic:   7f 45 4c 46 01 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF32
  Data:                              2's complement， little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              EXEC (Executable file)
  Machine:                           ARM
  Version:                           0x1
  Entry point address:               0x82e1
  Start of program headers:          52 (bytes into file)
  Start of section headers:          4508 (bytes into file)
  Flags:                             0x5000402， has entry point， Version5 EABI， hard-float ABI
  Size of this header:               52 (bytes)
  Size of program headers:           32 (bytes)
  Number of program headers:         9
  Size of section headers:           40 (bytes)
  Number of section headers:         30
  Section header string table index: 27

```
对比上一章可重定位文件的 elf 头，有以下几点不同:



* Type 由 REL (Relocatable file) 变成了 EXEC (Executable file)，表示这是一个可执行文件. 
* Entry point address，即程序入口为 0x82e1 而不再是 0，表示程序在加载时需要将入口代码放到该地址执行.  
* 多了一个 Program header，起始偏移地址为 52，紧随着 elf header.  
* section 的数量增加到了 30 个，这是因为程序在链接过程中不仅仅包含用户编写的源代码，还会链接 glibc 库，增加的那些 section 从 glibc 而来，同时增加了 9 个 Program header，表示该程序有 9 个 segment.  



### segment
紧接着 elf header 的部分是 Program headers，用于描述 segments，同样使用 readelf 命令查看相应信息:

```
readelf -l foo

Elf file type is EXEC (Executable file)
Entry point 0x82e1
There are 9 program headers， starting at offset 52

Program Headers:
  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
  EXIDX          0x00041c 0x0000841c 0x0000841c 0x00008 0x00008 R   0x4
  PHDR           0x000034 0x00008034 0x00008034 0x00120 0x00120 R E 0x4
  INTERP         0x000154 0x00008154 0x00008154 0x00019 0x00019 R   0x1
      [Requesting program interpreter: /lib/ld-linux-armhf.so.3]
  LOAD           0x000000 0x00008000 0x00008000 0x00428 0x00428 R E 0x8000
  LOAD           0x000f0c 0x00010f0c 0x00010f0c 0x0011c 0x00128 RW  0x8000
  DYNAMIC        0x000f18 0x00010f18 0x00010f18 0x000e8 0x000e8 RW  0x4
  NOTE           0x000170 0x00008170 0x00008170 0x00044 0x00044 R   0x4
  GNU_STACK      0x000000 0x00000000 0x00000000 0x00000 0x00000 RW  0x10
  GNU_RELRO      0x000f0c 0x00010f0c 0x00010f0c 0x000f4 0x000f4 R   0x1

 Section to Segment mapping:
  Segment Sections...
   00     .ARM.exidx
   01
   02     .interp
   03     .interp .note.ABI-tag .note.gnu.build-id .gnu.hash .dynsym .dynstr .gnu.version .gnu.version_r .rel.dyn .rel.plt .init .plt .text .fini .rodata .ARM.exidx .eh_frame
   04     .init_array .fini_array .jcr .dynamic .got .data .bss
   05     .dynamic
   06     .note.ABI-tag .note.gnu.build-id
   07
   08     .init_array .fini_array .jcr .dynamic

```
相对于 section 而言，对 segment 会更陌生一些，该文件中一共 9 个 segment，各个 segment 对应不同的类型(type)和不同的读写权限，这些 segment 将被加载到其对应的虚拟地址上，参与程序的执行过程，由于本文只关注 elf 文件的框架，对于各 segment 的属性我们将在后文中详细探讨.  

同时， readelf 工具还列出了 section 到 segment 的映射表，正如上文所说，segment 是从程序加载的角度来对各个 sections 进行组合并使用 Program headers 进行描述，所以在 elf 文件中， sections 放在 Program headers table 和 section header table 之间，而 Program headers table 和 section header table 只是以不同的角度对 sections 进行描述，一个针对加载，一个针对链接.

可重定位目标文件没有 Program header table，而可执行文件在加载时并不需要用到 section header table.  




### sections
紧接着 Program header table 的部分就是各 sections 的实际数据了，根据 elf 头可以看到，该 elf 文件有 30 个 section，我们通过 readelf 查看 sections header table 来获取 sections 的基本信息:

```
readelf -S foo

There are 30 section headers， starting at offset 0x119c:

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
Key to Flags:
  W (write)， A (alloc)， X (execute)， M (merge)， S (strings)
  I (info)， L (link order)， G (group)， T (TLS)， E (exclude)， x (unknown)
  O (extra OS processing required) o (OS specific)， p (processor specific)

```
不知道你有没有发现，在可重定位目标文件中，sections 是以 .text ， .data， .data，...为顺序排列的，但是在可执行文件中，它的排列顺序正好迎合了 sections 到 Program header table 的映射. 当然，相同属性的段组成同一个 segment，在内存地址上肯定也需要连续存放，这样在加载时才会方便进行数据的直接 copy.   

同样的，对于各段的分析我们将在后续的文章中详细讨论.  



## elf 可执行文件布局验证
为了弄清楚 elf 可执行文件是不是像上文图中显示的那种布局，我们还需要进行相应的验证，以确保我们是不是忽略了某些重要的内容:



* 首先，elf 头部占 52 字节.
* 紧接着 program header table 的起始地址为 52，每一个 program header 条目长度为 32，共 9 个，即 52+32*9 = 340.对应的 16 进制为 0x154.
* 随后是各个段的数据，放置在最前面的段是 .interp，这里我们只需要关注 section 的 offset 属性，因为这是当前文件中的 offset.
* 通过 elf 头可以得到：section headers table 的起始地址为 4508，长度为 40*30，对应的结束地址为 5708.十六进制为 0x164c.
* 最后依然是 .symtab 和 .strtab，从 0x164c 开始，由 0x1cfc+0x371=0x206d，对应十进制为 8301.
* 使用 ls -l 查看 foo 文件，其文件长度正好等于 8301，因此可以确认整个 elf 的布局正如前文所说的，丝毫不差。  



### 参考

[binutils源码](https://ftp.gnu.org/gnu/binutils/)

[arm elf 文档](https://static.docs.arm.com/ihi0044/g/aaelf32.pdf)

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。