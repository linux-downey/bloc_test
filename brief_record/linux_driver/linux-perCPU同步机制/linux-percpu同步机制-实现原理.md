# linux-percpu 同步机制 -- 实现原理

上一章对 percpu 同步机制进行了简单的介绍，并讨论了如何使用该机制，这一章，我们将要继续深入，探寻它背后的实现原理。  



## percpu 变量的定义
percpu 变量的静态定义是通过 DEFINE_PER_CPU(type,name) 接口，它的源代码实现如下：

```C
#define DEFINE_PER_CPU(type, name)					\
	DEFINE_PER_CPU_SECTION(type, name, "")

#define DEFINE_PER_CPU_SECTION(type, name, sec)				\
	__PCPU_ATTRS(sec) PER_CPU_DEF_ATTRIBUTES			\
	__typeof__(type) name

#define __PCPU_ATTRS(sec)						\
	__percpu __attribute__((section(PER_CPU_BASE_SECTION sec)))	\
	PER_CPU_ATTRIBUTES

#ifdef CONFIG_SMP
    #define PER_CPU_BASE_SECTION ".data..percpu"
#else
    #define PER_CPU_BASE_SECTION ".data"
#endif

```
经过一番兜兜转转的追踪，找到了 DEFINE_PER_CPU 的定义，将宏展开的结果就是：

```C
#define DEFINE_PER_CPU(type, name)					\                               
    __percpu __attribute__((section(".data..percpu"))) type name;  \ 
#else   \
    __percpu __attribute__((section(".data"))) type name; \
#endif
```
整个实现翻译过来就是：在 SMP 架构下，被定义的 percpu 变量在编译后放在 .data..percpu 这个 section 中，在单核系统中， percpu 变量被放在 .data 也就是数据段中，所以单核系统下的 percpu 变量直接被当成普通变量进行处理。这里我们主要关注的是 SMP 架构下的 percpu 变量处理。  
## section的处理与链接
既然涉及到 section 的定位，自然需要查看 linux 的链接处理，链接处理和平台是强相关的，这里我们以 arm 平台为例：  

在链接脚本 arch/arm/kernel/vmlinux.lds 中，对 .data..percpu section 进行了重定位(为该 section 分别加载地址)。

```
...
. = ALIGN((1 << 12)); 
.data..percpu : AT(ADDR(.data..percpu) - 0) {
     __per_cpu_load = .; __per_cpu_start = .; __per_cpu_user_mapped_start = .; 
     *(.data..percpu..first) . = ALIGN((1 << 6)); 
     *(.data..percpu..user_mapped) 
     *(.data..percpu..user_mapped..shared_aligned) . = ALIGN((1 << 12)); 
     *(.data..percpu..user_mapped..page_aligned) __per_cpu_user_mapped_end = .; . = ALIGN((1 << 12)); 
     *(.data..percpu..page_aligned) . = ALIGN((1 << 6)); 
     *(.data..percpu..read_mostly) . = ALIGN((1 << 6)); 
     *(.data..percpu) 
     *(.data..percpu..shared_aligned) __per_cpu_end = .; 
}
```  
其中，"." 在链接脚本中用作定位符，也就是通过 "." 来确定符号的加载位置，"." 的值是自动更新的，比如下列的脚本：

```
. = 0x1000
.text : { *(.text) }
```
在执行完这个指令之后 . 的位置就成了 0x1000 + sizeof(*.text)。

在链接脚本的前面部分 "." 已经被赋值了，而且添加了不少的段，我们暂时不去管 . 的值(注1)，主要看段的内容：

在重定位中，.data..percpu 段被重新定义，从开始的位置依次存放源代码代码中定义的放在 .data..percpu..first(section)、.data..percpu..user_mapped(section)、...、.data..percpu(section)、.data..percpu..shared_aligned(section) 这些自定义段中的 percpu 变量。  

同时，定义了几个变量：__per_cpu_load、__per_cpu_start、__per_cpu_user_mapped_start、__per_cpu_end，分别对应 percpu 的加载地址、percpu 变量内存开始地址、用户映射开始地址、percpu 变量内存结束地址。  

在上文的介绍中，我们只讨论了 DEFINE_PER_CPU 这个系统提供的最基础的也是用得最广泛的接口，事实上除了这个，内核还提供一些特殊场景下使用的接口：

```
//该接口定义的 percpu 变量将被放在 .data..percpu..first section 中，这些 percpu 变量始终放在 percpu 内存的头部位置
DEFINE_PER_CPU_FIRST(type, name)

//该接口定义的 percpu 变量将被放在 .data..percpu..user_mapped..shared_aligned 中，这些 percpu 变量在 SMP 的情况下将会对齐到 L1 cache，单核 CPU 的情况下则不对齐
DEFINE_PER_CPU_SHARED_ALIGNED(type, name)

//该接口定义的 percpu 变量将被放在 .data..percpu..page_aligned 中，无论 SMP 还是单核CPU，都会对齐到 L1 cache。
DEFINE_PER_CPU_ALIGNED(type, name)


//该接口定义的 percpu 变量将被放在 .data..percpu..read_mostly 中。  
DEFINE_PER_CPU_READ_MOSTLY(type, name)
```
这些接口定义的 percpu 变量都将被加载到 .data..percpu section 开始的内存。  


注1：如果你想确认当前平台下 .data..percpu section 重定位的位置，除了从链接脚本进行推理，也可以直接在 System.map 文件中搜索 __per_cpu_load 或者 __per_cpu_start 符号对应的地址即可，这个地址和内核在 ram 中的加载位置相关联。  








参考：[蜗窝科技](http://www.wowotech.net/kernel_synchronization/per-cpu.html)

