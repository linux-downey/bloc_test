# linux 内核中的上下文判断
涉及到中断系统的讨论难免要涉及到上下文的概念，上下文也就是代表着程序执行的环境，在特定的环境下只能做特定的事，或者不允许做某些事。一个最常见的例子是：在中断上下文中不允许睡眠。  

那么，上下文的设置到底是如何进行的？是由硬件控制的还是由软件来实现管理的？一切还是要从源代码中寻找答案。  




## 进程的 preempt_count 变量

### thread_info
在内核中，上下文的设置和判断接口可以参考 include/linux/preempt.h 文件，整个机制的实现都依赖于一个变量：preempt_count，这个变量被定义在进程的  thread_info ，也就是线程描述符中，线程描述符被放在内核栈的底部，在内核中可以通过 current_thread_info() 接口来获取进程的 thread_info:

```
static inline struct thread_info *current_thread_info(void)
{
	return (struct thread_info *)
		(current_stack_pointer & ~(THREAD_SIZE - 1));
}

register unsigned long current_stack_pointer asm ("sp");

```
其中 THREAD_SIZE 通常的大小为 4K 或者 8K，取出当前内核栈的 sp 指针，通过简单地屏蔽掉 sp 的低 13 位，就可以获取到 thread_info 的基地址。  

在 arm 中，preempt_count 是 per task  的变量，而在 x86 中，preempt_count 是 percpu 类型的变量。



### preempt_count
作为控制上下文的变量, preempt_count 是 int 型，一共 32 位。通过设置该变量不同的位来设置内核中的上下文标志，包括硬中断上下文、软中断上下文、进程上下文等，通过判断该变量的值就可以判断当前程序所属的上下文状态。  

preempt_count 中 32 位的分配是这样的：TODO

如图所示，整个 preempt_count 被分为几个部分：
* bit0~7：这八位用于抢占计数，linux 内核支持进程的抢占调度，但是在很多情况下我们需要禁止抢占，每禁止一次，这个数值就加 1，在使能抢占的时候将减 1，系统支持的最大嵌套次数为 256 次。  
* bit8~15：描述软中断的标志位，因为软中断在单个 cpu 上是不会嵌套执行的，所以只需要用第 8 位就可以用来判断当前是否处于软中断的上下文中，而其它的 9~15 位用于记录关闭软中断的次数。  
* bit16~19：描述硬中断嵌套次数的，在老版本的 linux 上支持中断的嵌套，但是自从 2.6 版本之后内核就不再支持中断嵌套，所以其实只用到了一位，如果这部分为正数表示在硬件中断上下文，为 0 则表示不在。  
* bit20 ：用于指示 NMI 中断，只有两个状态：发生 NMI 中断置 1，退出中断清除。  
* bit31 ：用于判断当前进程是否需要重新调度，但是该标志位目前没有使用，改用了另一个标志位 TIF_NEED_RESCHED 来指示。  



## 上下文的操作接口

内核提供了一系列的对上下文的设置和查询操作，这些接口都是通过操作 preempt_count 来实现，需要注意的是，所有的这些对于上下文的操作都是基于 local cpu 的。  

### preempt_count 基础操作
对 preempt_count 的操作有以下几个函数：

```
preempt_count_sub(val)     //将 preempt_count 减去 val
preempt_count_add(val)     //将 preempt_count 加上 val
preempt_count_inc()        //将 preempt_count 加上 1    
preempt_count_dec()        //将 preempt_count 减去 1    
```

### 内核抢占操作
内核抢占的关闭和开启分别通过下面两个函数：

```
preempt_disable()
preempt_enable()
```

####  preempt_disable
preempt_disable 的实现是这样的：

```
#define preempt_disable() \
do { \
	preempt_count_inc(); \
	barrier(); \
} while (0)
```

preempt_count_inc 函数就是对 preempt_count 变量加 1，因为抢占计数器位于 0~7 位，所以可以直接执行加 1 操作。  

#### preempt_enable
而 preempt_enable 的实现是这样的：

```
#define preempt_enable() \
do { \
	barrier(); \
	if (unlikely(preempt_count_dec_and_test())) \
		__preempt_schedule(); \
} while (0)
```

preempt_enable 将 preempt_count 减一并且判断 preempt_count 是否为 0，表示既不在中断环境下，也没有禁止中断抢占，这种情况下再判断当前进程是否设置了 TIF_NEED_RESCHED 标志，如果设置了，就触发一次进程调度。  



### 软中断接口

#### 获取软中断 count
获取软中断的 count 通过 softirq_count 函数：

```c++
#define softirq_count()	(preempt_count() & SOFTIRQ_MASK)                   //等价于 preempt_count & 0xff00
#define SOFTIRQ_MASK	(__IRQ_MASK(SOFTIRQ_BITS(8)) << SOFTIRQ_SHIFT(8))  //等价于 0xff << 8 = 0xff00;
#define __IRQ_MASK(x)	((1UL << (x))-1)                                   //等价于 (1<<8) - 1,也就是 0xff
#define SOFTIRQ_SHIFT	(PREEMPT_SHIFT + PREEMPT_BITS(8))
#define PREEMPT_BITS	8
#define PREEMPT_SHIFT	0
#define SOFTIRQ_BITS	8
```

经过解析，发现 softirq_count() 的返回值并非我们想象中的禁用下半部的次数，如果需要获取该次数，需要将结果右移 8 位，只不过通常情况下，我们并不需要这么干，通过内核提供的接口就可以做大部分的操作。    

#### 判断是否处于 softirq 上下文
in_softirq 函数用于判断目前程序是否运行在 softirq 上下文中，该函数的定义为：

```
#define in_softirq()		(softirq_count())
```

但是处于 softirq 上下文并不意味着程序正在执行软中断，因为软中断的 disable 也会导致 softirq_count() 不为0，如果我们判断当前是否正在软中断上下文，可以使用 in_serving_softirq*():

```c++
#define in_serving_softirq()	(softirq_count() & SOFTIRQ_OFFSET)   // softirq_count() & 1<<8
#define SOFTIRQ_OFFSET	(1UL << SOFTIRQ_SHIFT)              // SOFTIRQ_SHIFT 为 8
#define SOFTIRQ_SHIFT	(PREEMPT_SHIFT + PREEMPT_BITS)
#define PREEMPT_BITS	8
#define PREEMPT_SHIFT	0
```

正如上文中我们对 preempt_count 的介绍，bit8 表示当前是否有软中断在执行，而 bit9 ~ bit15 表示软中断被禁止的次数，所以如果需要判断是否有软中断正在执行，需要将 softirq_count() & 1<< 8;


#### softirq 的 enable

local_bh_enable 实现源码如下：

```c++
static inline void local_bh_enable(void)
{
	__local_bh_enable_ip(_THIS_IP_, SOFTIRQ_DISABLE_OFFSET);
}
static void __local_bh_enable(unsigned int cnt)
{
	...
	preempt_count_sub(cnt);
}

```
其中 SOFTIRQ_DISABLE_OFFSET 为 (2 * SOFTIRQ_OFFSET)，在上面可以看到 SOFTIRQ_OFFSET 的值为 1 << 8,所以 SOFTIRQ_DISABLE_OFFSET 的值为 1 << 9.  

preempt_count_sub(SOFTIRQ_DISABLE_OFFSET) 函数将 preempt_count 减去 1 << 9,也就是将 bit9 ~ bit15 组成的数字减一。

#### softirq 的 disable

不难猜到 local_bh_disable 与  local_bh_enable 是相对立的：

```c++
static inline void local_bh_disable(void)
{
	__local_bh_disable_ip(_THIS_IP_, SOFTIRQ_DISABLE_OFFSET);
}
static __always_inline void __local_bh_disable_ip(unsigned long ip, unsigned int cnt)
{
	preempt_count_add(cnt);
	barrier();
}
```
源码也证实了这一点，local_bh_disable 执行的操作就是 preempt_count_add(SOFTIRQ_DISABLE_OFFSET)，即 preempt_count + 1 << 9;

#### 进入软中断的处理

preempt_count 的 bit8 指示当前是否有软中断处于执行状态，这个 bit 由 __local_bh_disable_ip 实现，这个接口通常用户不需要直接使用到，它会在软中断的处理函数 __do_softirq 中被调用：

```c++
__local_bh_disable_ip(_RET_IP_, SOFTIRQ_OFFSET);
```
表示 preempt_count + 1 << 8;

在系统执行完软中断之后，再调用 __local_bh_enable(SOFTIRQ_OFFSET)，将 bit8 清除。  



### 硬中断接口
硬件中断虽然占据 bit16 ~ bit19，因为新内核不支持中断嵌套，所以只使用了 bit16，其它三位属于冗余部分。  


#### 上下文的设置
硬件中断的 preemt_count 的设置是通过 irq_enter 和 irq_exit 函数来实现的,这两个函数最终分别会调用以下两个接口：

```c++
#define __irq_enter()					\
	do {						\
		...
		preempt_count_add(HARDIRQ_OFFSET);	\
		...
	} while (0)

#define __irq_exit()					\
	do {						\
		...
		preempt_count_sub(HARDIRQ_OFFSET);	\
	} while (0)
```

其中 HARDIRQ_OFFSET 宏展开为 1 << 16,也就是 bit16，既然不涉及到中断嵌套，而且 irq_enter 和 irq_exit 是成对出现的，所以所有的操作都是针对于 bit16 的置位和清除。  

#### 硬中断上下文判断
用户可以使用 in_irq() 来判断是否处于中断上下文：

```
#define in_irq()		(hardirq_count())
#define hardirq_count()	(preempt_count() & HARDIRQ_MASK)
```
其中，HARDIRQ_MASK 是硬中断的位掩码，它的值是 0xf00，in_irq 就是判断 preempt_count & 0xf0000。  


### NMI 接口
NMI 对应 bit20，NMI 的全名为不可屏蔽中断，顾名思义，这一类型的中断是不能屏蔽的，只要产生了就必须得到处理，它一般会在系统发生了无法恢复的硬件错误的时候，通过 NMI pin 或者内部总线产生，比如芯片错误、内存校验错误，总线数据损坏等，这一类错误都会严重影响到系统运行。  

#### NMI 上下文判断
NMI 的上下文判断使用 in_nmi 接口，这个接口主要就是判断 preempt_count 的 bit20

```c++
#define in_nmi()		(preempt_count() & NMI_MASK)
#define NMI_MASK	(__IRQ_MASK(NMI_BITS)     << NMI_SHIFT)
```
其中，NMI_BITS 为 1，NMI_SHIFT 为 20。


### PREEMPT_NEED_RESCHED 标志
PREEMPT_NEED_RESCHED 对应 bit31，它负责管理当前进程是否需要被调度，当当前进程时间片用完了或者陷入睡眠，就会将当前进程的这个标志位置位，在中断返回、内核返回到用户空间、使能内核抢占等抢占调度点将会检查这个标志位以确定是否需要重新调度进程。  

这个标志位目前没有被使用，而是使用 TIF_NEED_RESCHED 来决定是否发生内核的抢占调度，这个标志位位于 thread_info->flags 的 bit1 。  



### 公共接口
除了针对硬中断、软中断和preemp(抢占)的单独接口，内核还提供一些公共的接口：

#### in_interrupt
in_interrupt 用于判断是否在中断上下文，包括软、硬中断：

```c++
#define in_interrupt()		(irq_count())
#define irq_count()	(preempt_count() & (HARDIRQ_MASK | SOFTIRQ_MASK | NMI_MASK))
```

不难看出，它的返回值就是 preempt_count | 0xf0000(HARDIRQ_MASK) | 0xff00(SOFTIRQ_MASK) | 0x100000。  

#### in_task
判断当前执行环境是否处于进程上下文环境，简单来说就是判断当前是否处于中断环境，然后将结果取反即可。  

```
#define in_task()		(!(preempt_count() & (NMI_MASK | HARDIRQ_MASK | SOFTIRQ_OFFSET)))
```





