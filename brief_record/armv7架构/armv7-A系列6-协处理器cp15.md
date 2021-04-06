# armv7-A系列6-协处理器cp15 

协处理器，顾名思义就是协助型处理器，主要协助做一些主处理器无法执行或者执行效率不佳的事情，比如浮点、图像、音频处理这一类，随着硬件的发展，大多协处理器的功能都慢慢集成到主处理器中，但是某些特定的工作还是需要协处理器进行辅助。  

在 arm 的协处理器设计中，最多可以支持 16 个协处理器，通常被命名为 cp0～cp15，其中：



* cp15 提供一些系统控制功能，主要针对内存管理部分
* cp14 主要提供 debug 系统的控制
* cp10、cp11 两个协处理器一起提供了浮点运算和向量操作，以及高级的 SIMD 指令扩展。
* 其它作为保留

对于 armv7 而言，主要是 cp14 和 cp15，cp14 主要针对调试系统，而 cp15 主要针对内存管理功能，在本系列文章中我们并不准备讨论 debug 相关内容，所以我们主要关注协处理器 cp15.  

如果想要知道当前处理器是否集成了cp0～cp13协处理器，可以通过访问寄存器 CPACR 来确定。  


## cp15寄存器特性
cp15 一共有 16 个寄存器，通常是需要 PL1 特权级才能访问，armv7 的 cp15 寄存器都是复合功能寄存器，多种功能对应多个寄存器内存实体，由访问指令的参数来决定访问的是哪种功能对应的内存，这和我们之前接触到的 bank 概念有些类似。  

这是因为：对于 armv7 架构而言，A 系列和 R 系列是统一设计的，A 系列带有 MMU 相关的控制，而 R 系列带有 MPU 相关控制，针对不同的功能需要做区分，同时又因为协处理器 cp15 只支持 16 个寄存器，而需要支持的功能较多，所以通过同一寄存器不同功能的方式来满足需求。在指令的编码中，支持 16 个寄存器只需要使用 4 位，如果需要支持 32 个寄存器，就需要多使用一位寄存器位，要知道，对于指令编码而言，每一位的资源都是非常紧缺的，关于指令编码可以参考 [arm指令集编码](https://zhuanlan.zhihu.com/p/362760953)。  



## cp15寄存器的访问
armv7 中对于协处理器的访问，使用 mcr 和 mrc 指令，分别表示将 arm 核心寄存器中的值的写到 cp15 寄存器中和从 cp15 寄存器中读到 arm 核心寄存器中，大部分指令都需要在 PL1 以及更高的特权级下才能正常执行，这是因为 cp15 协处理器大多都涉及到系统和内存的设置，user 模式没有操作权限，user 模式仅能访问 cp15 中有限的几个寄存器比如：ISB、DSB、DMB、TPIDRURW、TPIDRURO 寄存器。  

以 mcr 为例，指令的参数稍微有些复杂：

```assembly
MRC<c> <coproc>, <opc1>, <Rt>, <CRn>, <CRm>{, <opc2>}
```


* <c\>：指令后缀，表示条件执行，关于条件执行可以参考 [arm状态寄存器](https://zhuanlan.zhihu.com/p/362687525)
* \<coproc\>:协处理器的名称，cp0～cp15 分别对应名称 p0～p15
* \<opc1\>:对于 cp15 而言，这一个参数填 0 即可。
* \<Rt\>:arm 的通用寄存器
* \<CRn\>:与 arm 核心寄存器交换数据的核心寄存器名，c0～c15
* \<CRm\>:需要额外操作的协处理器的寄存器名，c0～c15，针对多种功能的 cp15 寄存器，需要使用 CRm 和 opc2 来确定 CRn 对应哪个寄存器实体。
* \<opc2\>：可选，与 CRm搭配使用，同样是决定多功能寄存器中指定实体。  

公式总是很抽象难懂的，我们通过一个例子来建立一个基本的印象：

```
mrc     p15, 0, r0, c0, c1, 1
```
这条指令的含义是将 cp15 ID_PFR1 寄存器中的值读取到 arm 的 r0 寄存器中，其中 c0，c1,1 三个参数共同用来指定目标寄存器是 ID_PFR1(参数与寄存器的对应需要通过查询手册获取，见下文)。  



## 寄存器总览
尽管 cp15 的寄存器是 c0～c15，但是根据上文中的介绍，实际上真正的寄存器实体完全不止 16 个，而是通过 \<opc1\>(通常是0) \<CRn\>, \<CRm\>{, \<opc2\>} 来确定需要操作的目标寄存器，下表就是整个 c0～c15 寄存器以及功能的总览：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/cp15%E6%89%80%E6%9C%89%E5%AF%84%E5%AD%98%E5%99%A8%E6%80%BB%E8%A7%88%E5%9B%BE.jpg)

### c0
c0 ID 寄存器是一个复合功能寄存器，对应的功能列表如下表：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/cp15-c0-ID%E5%AF%84%E5%AD%98%E5%99%A8.jpg)

### c1
c1 为系统控制寄存器，同样也是一个复合功能寄存器，对应的功能如下表：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/cp15-c1-%E7%B3%BB%E7%BB%9F%E6%8E%A7%E5%88%B6%E5%AF%84%E5%AD%98%E5%99%A8.jpg)

系统控制寄存器中需要中断关注的是 SCTLR，即 mrc 指令在 CRn，opc1、CRm、opc2 的值分别为 c1、0、c0、0 时操作的寄存器，它的部分寄存器位定义是这样的：



* TE:bit[30],配置在发生异常时，包括 reset，将会进入哪种指令集，0 - arm 指令集，1 - thumb 指令集。  
* NMFI，bit[27]，软件是否可以配置 fiq 掩码， 0 - 软件可以通过写 CPSR.F 位来屏蔽 fiq， 1 - 不能配置。
* EE，bit[25]，在进入异常处理时的大小端配置，0 - 小端，1 - 大端
* FI，bit[21]，快中断配置使能。
* V，bit[13]，配置中断向量表的位置，0 - 0x00000000， 1 - 0xffff0000
* I，bit[12]，指令cache控制，0 - 指令cache disabled，1 - 指令 cache enable。
* Z，bit[11]，分支预测控制，0 - 分支预测功能 disabled，1 - 分支预测功能enable。
* C，bit[2]，cache 控制，0 - 数据 cache 和 unified cache disabled，1 则反之。
* A，bit[1]，对齐检查， 0 - 对齐错误检查 disabled，当此位为 0 时，访问非对齐内存不报错。  
* M，bit[0]，MMU 使能控制， 0 - MMU disabled， 1 - MMU enabled。  

SCTLR 控制系统的一些核心功能，包括 MMU、大小端、指令集等配置。



### c2、c3
c2、c3寄存器提供内存的保护和控制功能，如果处理器实现了物理地址扩展功能，c2 寄存器中的 TTBR0、TTBR1、HTTBR、VTTBR 寄存器实体将会扩展为 64 位寄存器。对应寄存器功能如下表所示：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/cp15-c2-c3-%E5%86%85%E5%AD%98%E4%BF%9D%E6%8A%A4%E6%8E%A7%E5%88%B6%E5%AF%84%E5%AD%98%E5%99%A8.jpg)

### c4
未使用

### c5、c6
c5、c6 寄存器主要提供内存系统的错误上报功能，对应的功能列表如下：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/cp15-c5-c6-%E5%86%85%E5%AD%98%E9%94%99%E8%AF%AF%E4%B8%8A%E6%8A%A5%E5%AF%84%E5%AD%98%E5%99%A8.jpg)

这一类寄存器在软件排错时可以提供非常大的帮助，比如通过 DFSR(数据状态寄存器)、IFSR(指令状态寄存器) 的 status bits 可以查到系统 abort 类型，内核中的缺页异常就是通过该寄存器传递异常地址，从而分配页面的。

### c7
c7 寄存器提供包括缓存维护、地址转换等功能，对应的寄存器功能见下表：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/cp15-c7-cache-address-trans.jpg)

### c8
c8 寄存器提供 TLB 维护功能，对应的寄存器功能见下表：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/cp15-c8-TLB-maintain.jpg)

### c9
c9 寄存器主要为 cache、分之预测 和 tcm 保留功能，这些保留功能由处理的实现决定，对应的寄存器功能见下表：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/cp15-c9-reserved-cache-tcm.jpg)

### c10 
c10 寄存器主要提供内存重映射和 TLB 控制功能，对应的寄存器功能见下表：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/cp15-c10-remap-tlb.jpg)

### c11
c11 寄存器主要提供 TCM 和 DMA 的保留功能，这些保留功能由处理的实现决定：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/cp15-c11-reserved-TCM-DMA.jpg)

### c12
c12 寄存器主要提供安全扩展功能，暂不讨论

### c13
c13 寄存器提供进程、上下文以及线程ID处理功能:

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/cp15-c13-process-context-threadId.jpg)

### c14
c13 寄存器提供通用定时器扩展的保留功能，这些保留功能由处理的实现决定，对应的寄存器功能见下表：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/cp15-c14-Generic-time.jpg)



### c15 

由处理器实现决定。  


从上述的协处理器寄存器列表可以看出，每个寄存器都对应多种功能，通过 mcr 或者 mrc 指令的参数来指定需要操作的具体寄存器，同时，cp15 协处理器的大部分寄存器功能都是和系统的内存相关，比如 MMU、TCM、TLB、DMA、Memory protection 等等。  

因为篇幅原因，博主不可能将所有的寄存器实体对应的寄存器详情列出来，关于寄存器的细节，可以查阅官方文档 armv7-AR 手册 - B3.17 章节。  



### 参考

[armv7-A-R 参考手册](https://gitee.com/linux-downey/bloc_test/blob/master/%E6%96%87%E6%A1%A3%E8%B5%84%E6%96%99/armv7-A-R%E6%89%8B%E5%86%8C.pdf)



[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。





