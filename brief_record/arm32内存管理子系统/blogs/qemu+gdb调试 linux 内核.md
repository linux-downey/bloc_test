# qemu + gdb 调试 arm linux 内核

很早就有研究 linux 启动代码的想法，最近正好借着研究 linux 内存管理子系统，一起看看内核部分的启动代码，毕竟内核的初始启动部分和内存是紧密相关的。 

原本是打算像研究其它内核子系统一样，打开代码准备细细研读，发现事情并没有那么简单，内核启动部分包含大量的汇编代码，同时还有大量的宏定义，在代码分析时只要搞错其中一个过程，后面就会相去甚远，同时，因为内核启动部分的调试系统还没有初始化，串口还不能使用。当然，在 MMU 没有打开之前，可以直接往串口数据寄存器中写数据进行调试，或者在 MMU 之后，手动地建立串口的虚拟地址映射，不过鉴于我比较懒，因此还是想找找其他能偷懒的方式。 

最终的解决方案是 qemu+gdb 对内核的启动进行调试，因为内核的启动是线性运行的，同时对于 gdb 的支持让我对 qemu 产生相见恨晚的感觉，这是继 ftrace 用来发现的第二大调试神器。 



## 环境准备

使用 qemu+gdb 调试 linux 内核，其实网上已经有很多教程了，本来想直接转载别人的博客就行了，在网上搜了半天也没看到写的比较合适的，大多数都是一篇文章大家转来转去，又或者是只贴出一个过程，不交代原理，这就像很多博客中就是把代码一贴就完事，这样还是不大好。

想一想还是自己写吧，也当是做个记录。 

要使用 qemu + gdb 调试内核，首先自然是需要安装这两者，qemu 和 gdb 的安装其实没什么好说的，因为我使用的是比较新的 ubuntu18.04，直接使用下面的命令就可以安装好这两者：

```
sudo apt-get install qemu
sudo apt-get install gdb
```

如果你使用不同的平台或者不同版本的 linux 发行版，安装过程中出现什么问题，理论上在网上都能找到解决方案，对于一个使用 linux 的开发者来说，这不应该是个问题。 

在我的虚拟机上，最后安装的版本是：

```
$ qemu-system-arm --version
QEMU emulator version 2.11.1(Debian 1:2.11+dfsg-1ubuntu7.36)
Copyright (c) 2003-2017 Fabrice Bellard and the QEMU Project developers

$ gdb --version
GNU gdb (Ubuntu 8.1.1-0ubuntu1) 8.1.1
```

qemu 支持多个平台的模拟，我们需要模拟的是 arm 平台，因此使用 arm-system-arm 这个可执行文件，在安装时该可执行文件就已经被添加到环境变量中。 

**至于 qemu 是什么，可以参考 [qemu 的 wiki](https://zh.wikipedia.org/wiki/QEMU)，wiki 中对 qemu 有一个基本的介绍，当然，除了对 qemu 有一个概念性的了解，还需要简单地了解它是怎么用的，这就涉及到 qemu 的[官方文档](https://qemu.readthedocs.io/en/latest/specs/index.html)**

**其中比较主要的是 [quick start](https://qemu.readthedocs.io/en/latest/system/quickstart.html) 和  [Invocation(主要是参数列表)](https://qemu.readthedocs.io/en/latest/system/invocation.html)**

**gdb 的使用在本篇博客中也不做过多介绍，相关的资料网上有很多**



## 内核镜像

通常使用 gdb 都是调试应用程序，如果需要对应用程序进行调试，比如添加 -o 参数，在编译时在应用程序的可执行文件中添加调试信息，gdb 的运行依赖于这些调试信息。 

内核也是一样的，默认编译的内核中自然是不带调试信息的，毕竟内核占用的可都是系统中宝贵的内存资源，能少用一些就少用一些，内核对于资源占用的优化可以从内核中定义的那些 \_\_init、\_\_exit 前缀函数看出，这些函数在内核中只执行一次，内核在使用完它们之后就把它们都扔了，只留下核心的部分，说是过河拆桥也不为过，最后只是为了降低内核镜像的内存占用。 

既然默认情况下不添加内核调试信息，那么自然是需要对内核进行配置再重新编译，如果你没有其它特定的调试需求，只需要增加下面的内核配置即可：

```
make menuconfig
	Kernel Hacking
		Compile-time checks and compiler options
			[*] Compile the kernel with debug info
```

Kernel Hacking 目录中针对各个组件有不同的 debug 选项，如果有兴趣可以自行研究。

**修改配置之后重新编译，最后生成的 zImage 或者 bzImage 就是我们需要的内核镜像。** 

通过对比不难发现，在我的 4.9 内核中，没有添加调试信息编译出来的 vmlinux 目标文件大小为 20M，而添加了调试信息的 vmlinux 编译出来为 185M，区别非常明显。 

 

## qemu 启动内核

鉴于只需要调试内核的启动部分，我也就没有制作 rootfs 从而模拟一个完整的 linux 虚拟机，仅仅是调试内核部分。 

qemu 模拟启动内核的命令为：

```
qemu-system-arm -S -s -m 1.5G -M sabrelite -kernel arch/arm/boot/zImage -dtb arch/arm/boot/dts/100ask_imx6ull-14x14.dtb
```

对应的参数含义如下(详情可查询[官方文档的Invocation section](https://qemu.readthedocs.io/en/latest/system/invocation.html))：

* -S : 在 qemu 环境准备好之后，系统并不向下执行，而是停在第一条指令处，等待用户操作
* -s：这个参数是 "-gdb tcp::1234" 的简写，针对 gdb 的远程调试功能，指定端口为 1234，其它的 gdb 客户端可以通过该端口进行连接，输入操作指令进行调试。
*  -m：指定模拟系统使用的内存大小，接受 M 和 G 为单位。**从理论上来说，对于 linux 而言，系统占用多大的内存是需要 linux 内核启动之后通过解析 dtb 文件获取的，但是 qemu 虚拟机启动时需要指定内存大小，如果不进行指定，默认的值是 128M。同时，根据我的实际测试，内核在启动时并不会像真实机器那样以设备树的指定来初始化内存，而是根据用户传入的 -m 参数来确定内存，这部分会在后续的代码中分析。**
* -M：-machine 的缩写，我模拟的是 imx6ull 的内核镜像，因此，先通过  **qemu-system-arm -S help** 命令找到 imx6ull 的 machine 参数名为 sabrelite，然后添加到 -M 参数后面，**qemu-system-arm -S help** 命令输出的列表基本上涵盖了市面上的通用处理器，甚至还提供 qemu 调试版本，**如果你需要调试你的内核，务必先确定你使用内核对应的 machine 参数**。
* -kernel：指定需要模拟的内核镜像
* -dtb：指定设备树文件
* -nographic：内核启动之后，默认会使用一个图形界面，鉴于我们是使用 gdb remote 进行调试，并不需要用到该图形界面，这个参数可以 disable 图形界面，但是实测中发现如果使用了这个参数，我将无法使用 CTRL+C 来终止进程，至于具体原因也没有太多时间去深究，因此在调试的时候我也就没有添加这个参数。

除了上面用到的几个函数，还可能使用的参数有：

* -smp ： 模拟多核系统

* -numa：模拟 numa 系统

  



## gdb 连接 qemu 

qemu 启动内核之后会停在内核开始处，在另一个终端启动 gdb 调试程序。

需要注意的是，如果内核是使用交叉编译工具链编译的，那么 gdb 也需要使用交叉编译工具链中提供的，对于 imx6ull，对应的指令为，符号表从编译内核时生成的  vmlinux 中提取：

```
$ arm-linux-gnueabi-gdb vmlinux
...
```

接着，在 gdb 中连接 qemu：

```
(gdb) target remote localhost:1234
Remote debugging using localhost:1234
0x10000000 in ?? ()
(gdb) 
```

如果不出意外， gdb 的连接就成功了，我们将关注点放到输出的这一行：

```
0x10000000 in ?? ()
```

不难看出，0x10000000 是一个内存地址，表示 qemu 将内核镜像拷贝到该地址上运行，在内核打开 mmu 之前，这就是一个物理地址，抛开虚拟机的概念，可以直接理解为 zImage 镜像被加载到了 0x10000000 这个物理地址上。 

后面的内容 "?? ()" 明显不像是正常的输出，理论上这里输出的应该是 0x10000000 地址上对应的内核符号，也就是内核 zImage 第一条指令对应的符号，这里明显是 qemu 没有找到对应的符号，这时候问题就来了，在编译内核的时候我们不是将符号添加到内核中了么，为什么还是找不到？

这个问题是否影响调试还不得而知，既然要调试内核，就继续往下走，通过阅读内核代码，找到内核入口为 _stext，于是在 _stext 处添加一个断点，然后执行单步调试：

```
(gdb) target remote localhost:1234
Remote debugging using localhost:1234
0x10000000 in ?? ()
(gdb) b _stext
Breakpoint 1 at 0xc0100000: file arch/arm/kernel/head.S, line 492.
(gdb) c
Continuing.
```

很遗憾地发现，断点是添加上了，但是压根就跳不到断点处，程序直接停在 Continuing 处了。 

既然不能设置断点，那我直接从第一条指令单步运行：

```
(gdb) target remote localhost:1234
Remote debugging using localhost:1234
0x10000000 in ?? ()
(gdb) s
Cannot find bounds of current function
```

竟然还是不能单步执行，那么这个问题到底是为什么呢？



## 内核的加载地址和虚拟地址

要搞清楚内核调试时无法执行的问题，需要对两个概念有所了解，即内核的加载地址和虚拟地址，这是程序链接过程中的概念。

通常情况下，程序的加载地址和虚拟地址是一样的，也就是直接把程序拷贝到它应该运行的地址上，但是内核的启动并不是如此，在编译阶段，内核为每个符号分配的是虚拟地址，也就是内核代码应该运行的位置，这是开启 MMU 之后的虚拟地址。

实际上在 uboot 加载内核镜像时，MMU 并没有开启，内核镜像此时面对的是赤裸裸的物理地址，对于不同的硬件，这个物理内存的地址区间很可能是不一样的，因此 uboot 无法保证内核的每个段被放到正确的地址上，而内核被 uboot 加载到内存中的地址被称为加载地址，如果加载地址和虚拟地址不相等，自然需要对代码进行重定位，将代码放到它正确的位置上。 

讲概念可能有点绕，我们来看看当前调试版本内核 vmlinux 的相关信息：

```
$ readelf -l vmlinux
...
Program Headers:
  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
  LOAD           0x000000 0xc0000000 0x80000000 0x0826c 0x0826c R E 0x10000
  LOAD           0x010000 0xc0100000 0x80100000 0x98a4d8 0x98a4d8 R E 0x10000
  LOAD           0x9a0000 0xc0b00000 0xc0b00000 0x3eaa1c 0x3eaa1c RWE 0x10000
  LOAD           0xd90000 0xffff0000 0xc0f00000 0x00020 0x00020 R E 0x10000
  LOAD           0xd91000 0xffff1000 0xc0f00020 0x002ac 0x002ac R E 0x10000
  LOAD           0xda02e0 0xc0f002e0 0xc0f002e0 0x802ac 0x802ac RWE 0x10000
  LOAD           0xe30000 0xc1000000 0xc1000000 0xb3000 0x12a4c8 RW  0x10000
  NOTE           0xd8a9f8 0xc0eea9f8 0xc0eea9f8 0x00024 0x00024 R E 0x4
  GNU_STACK      0x000000 0x00000000 0x00000000 0x00000 0x00000 RWE 0x10
  
  Section to Segment mapping:
  Segment Sections...
   00     .head.text 
   01     .text .fixup 
   ...  

```



readelf -l 是读取 vmlinux 中的 segment 相关信息，如上文中显示：

* Offset 表示该 segment 在文件中的偏移
* VirtAddr 表示该 segment 应该被加载的虚拟地址
* PhysAddr 表示 segment 对应的加载地址

尽管真正运行的是 zImage 而不是 vmlinux，对于内核中各个 segment 的分布，在编译成 vmlinux 时就已经确定了，zImage 只是做了一些裁剪、压缩等处理，因此，直接对 vmlinux 进行分析是没有问题的。

从上文中可以看出，各个 segment 之间的 Offset 和 PhysAddr 项是不相等的，这就意味着，内核的加载并不意味着简单的拷贝，因为如果是简单地拷贝镜像到内存中，第二个 segment 应该是在 0x80000000+Offset = 0x80010000 上，实际上却是在 0x80100000 上，这就意味着，内核开始部分的代码会被内核代码进行重定位。 

实际上也确实是这样的，无论是 zImage 还是 bzImage，前缀 z 代表内核是被压缩过的，内核镜像的起始代码就是解压缩代码，这部分代码不仅仅是解压内核，同时还要负责将后续的 segment 放在它合适的位置上，实现启动前的重定位操作。 

这里会存在一个问题，内核所有的代码在编译时，都是根据虚拟地址进行编译的，这个可以通过查看符号表或者链接脚本来确定，但是呢，内核在重定位也就是开启 MMU 之前的那部分代码肯定是没有运行在正确的地址上的，因此，这部分代码都是 PIC 代码，也就是这部分代码不依赖于运行地址，放到哪儿都能运行，PIC 代码参考 TODO.

让我们再回到最初的问题：内核镜像被 qemu 加载到 0x10000000 地址处，为什么找不到对应的符号？

这是因为在编译阶段所有的符号都是编译为虚拟地址的，比如内核入口 _stext：

```
$ readelf -s vmlinux | grep "_stext"
161580: c0100000     0 NOTYPE  GLOBAL DEFAULT    2 _stext
```

实际上，内核入口代码运行时肯定还没有开启 MMU，也就是说还在镜像的加载地址上，因此，在 qemu 中，_stext 内核代码位置实际位置可能在 0x10100000 上，而 vmlinux 中 _stext 编译时产生的符号地址在 0xc0100000 处，gdb 通过该地址自然查不到对应的符号，内核代码中所有开启 MMU 之前的代码都存在这个问题。



## 修改符号表

知道问题所在，问题也就比较好解决了：首先，gdb 读取到的符号表中符号的地址与符号真实所在的地址是有偏差的，好在 gdb 提供相应的指令可以对符号表进行修改，对应的指令为：add-symbol-file vmlinux。 

再次查看 vmlinux 中的 segtion 信息：

```
$ readelf -S vmlinux
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .head.text        PROGBITS        c0008000 008000 00026c 00  AX  0   0  4
  [ 2] .text             PROGBITS        c0100000 010000 98a4bc 00  AX  0   0 64
  [ 3] .fixup            PROGBITS        c0a8a4bc 99a4bc 00001c 00  AX  0   0  4
  [ 4] .rodata           PROGBITS        c0b00000 9a0000 3602a0 00  WA  0   0 4096
....
```

通过查看源代码可以看出，vmlinux 中前两个 section 中包含了开启 MMU 之前的代码，而开启 MMU 之后，代码就运行在其正确的虚拟地址之上了，符号表是直接可用的，因此，我们需要对前面两个 section 进行符号表的修改。 

通过上面 qemu 启动信息可以看出，镜像被加载到 0x10000000 地址上，因此 .head.text 的实际物理地址为 0x10008000，因此修改该 segtion 对应的符号表：

```
add-symbol-file vmlinux -s .head.text 0x10008000
```

而 .text section 对应的物理地址为 0x10100000，同样，修改该 segtion 对应的符号表：

```
add-symbol-file vmlinux -s .text 0x10100000
```

到此，通过 gdb 就可以设置开启 MMU 之前代码的断点了：

```
(gdb) print _stext
$1 = {<text variable, no debug info>} 0xc0100000 <__turn_mmu_on>
(gdb) b _stext
Breakpoint 1 at 0x10100000: _stext. (2 locations)
(gdb) c
Continuing.

Breakpoint 1, __turn_mmu_on () at arch/arm/kernel/head.S:492
492		instr_sync
(gdb) 
```

设置完成之后，qemu 调试内核就不再局限于 start_kernel 函数之后了，而是可以从内核入口 _stext 开始调试。 

而 start_kernel 之后的代码原本就是可以正常调试的。 



## 内核调试中可能碰到的问题



### 内核优化问题

内核编译时使用的默认优化等级为 O2，在该优化等级下，针对程序会执行相当多的优化，因此内核不会严格地按照代码的顺序运行，单步调试时看起来更是跳来跳去，非常不方便调试，但是我们并不能直接将内核的优化等级调到 O0，内核代码在 O0 优化等级下编译是会报错的，这是因为内核中有些代码是基于编译器会进行优化这种假定而编写的，因此将优化等级调成 O0 会编译不通过。 

### 修改文件优化等级

在需要单步调试的时候，自然是希望目标函数严格地按照代码运行，一种方法是直接修改文件的编译优化等级，假设需要修改 target.c 的优化等级，就在该文件对应的 Makefile 中添加：

```
CFLAGS_REMOVE_target.o = -O2
```

 表示移除 O2 优化等级，使用默认的 O0 优化等级，CFLAGS_REMOVE_ 这个前缀并不是 Makefile 语法支持的，而是 linux 的 Kbuild 系统提供的支持，在 scripts/Makefile.lib 中对 cflags 有以下处理：

```
_c_flags       = $(filter-out $(CFLAGS_REMOVE_$(basetarget).o), $(orig_c_flags))
```

该 _c_flags 最后将会在编译时作用于对应的文件，具体的实现就不过多赘述了。 



### 修改函数优化等级

如果不想直接将整个文件的编译等级降为 O0，又或者是整个文件使用 O0 等级编译不通过，gcc 提供了针对函数的编译等级优化设置：

```
 __attribute__((optimize("O0")))
```

在函数声明后添加该后缀，表明针对该函数使用 O0 优化等级，当然，也可以设置为其它的优化等级。 



### inline 设置

在内核编译过程中，为了提高效率，支持 inline 关键字将函数编译为内联函数，对于普通的函数调用而言，编译器生成的代码会为每个被调用函数在栈上分配空间，被调用函数执行完成之后回收栈上空间。

对于某些频繁调用且简短的函数，实际上可以直接将被调用函数直接展开到调用函数中，相当于被调用函数中的代码直接放到调用者中，也就免去了函数调用的那一套流程，节省栈空间的同时提升了效率。

但是在调试过程中，inline 关键字因为会直接把函数内容展开到调用者中，因此会丢失一些符号，导致调试的不方便，同时，并不一定只有显式指定 inline 的函数才会被编译为内联函数，编译器会自动检测并判断哪些函数可以被优化为内联函数，并直接对齐进行内联优化。 

因此，为了防止函数因为被编译为内联函数而丢失符号，内核中可以使用 noinline 在函数定义时修饰一个函数以达到目的，本质上这个关键字还是使用了 gcc 的  \_\_attribute\_\_ 关键字：

```
#define  noinline	__attribute__((noinline))
#define __always_inline	inline __attribute__((always_inline))
```

同样的，还有对应的 __always_inline。

如果你内核中的函数显式声明了 inline，为了调试方便，可以尝试删除 inline 关键字，不过在某些函数上可以编译报错。



### percpu 参数的处理

内核中为了更好地支持 CPU 之间全局参数的同步问题，引入了 percpu 机制，对于声明为 percpu 类型的数据，是和 CPU 相绑定的，每个 CPU 对应一份独立的数据，通常情况下，CPU 只会访问属于当前 CPU 的数据，但是内核并不禁止 CPU 访问其它 CPU 的 percpu 数据。

在实际使用 gdb 调试的过程中，会发现 percpu 类型的数据并不能被正常地调试，这是因为 percpu 机制其特殊的实现机制：声明为 percpu 的数据在编译期间将会被放在特殊的段内，内核启动时，将会对这些段进行处理，既然是 percpu 类型的，那么就是在启动时为每个 cpu 在内存中分配一份，每个 CPU 根据其 cpu id 获取对应的内存地址。

因此，CPU 在访问 percpu 数据时，实际上访问的不是编译时该数据的地址，而是内核启动时为每个 CPU 申请的内存数据，因此 gdb 从 vmlinux 读取到的符号地址并不对应真实的 percpu 数据地址。

举个例子：

```
在一个存在两个逻辑 CPU 的内核中，内核中定义了一个 percpu 类型的 value
在编译时分配的地址为 0xc0001000
那么在内核启动解析 percpu 变量时，会创造两份 value 的备份，一份地址为 0x10002000，给 cpu0 使用，一份地址为 0x10003000，给 cpu1 使用。
cpu0 运行时，默认操作的是 0x10002000 处的 value，cpu1 同理。
```

而在实际调试中，符号表保存的 value 对应的却是 0x10001000 处的原始数据，自然是不对的。那么，这个 0x10001000 处的原始数据还能不能用呢？它肯定是不会被各个 CPU 用到的，不但如此，在内核初始化完成之后，原始的 percpu 数据将会被 buddy 子系统回收。 

如果要正常调试 percpu 相关的数据，那应该怎么做呢？

其实我也没想到很好的办法，一个不那么优雅的方式就是，将 percpu 参数赋值给其它临时变量，在 O0 优化等级的情况下，可以通过临时变量间接访问 percpu 变量。 

当然，了解了 percpu 原理之后，也可以通过 add-symbol-file  修改符号表位置，不过我觉得这样太过于麻烦，通常调试也就使用上一种方式了。 







参考：

[内核优化等级](http://zhaoyujiu.com/2020/06/17/change-linux-gcc-optimalize-level/)：http://zhaoyujiu.com/2020/06/17/change-linux-gcc-optimalize-level/

qemu 官方文档：https://qemu.readthedocs.io/en/latest/specs/index.html