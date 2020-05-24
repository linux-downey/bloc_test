## 段信息
根据上文中的 elf 文件框架，在 elf 头部之后就是各 sections，section headers 按照一定的格式存放每个 section，包含具体的数据，比如 text 段中包含代码部分，数据、bss 段中包含数据。每种数据都有对应的段解析方式，这一部分我们将在后续的文章中详细解析。  

这一章我们主要研究 elf 目标文件的框架，所以需要获取段的描述信息，而段的描述信息被保存在 section headers 中，每一个段都由一个 section header 来描述，如果说 elf 头部是整个 elf 文件的"目录"，那么 section headers 就是文件内段信息的"目录"。 

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




