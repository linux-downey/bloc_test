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

### preempt_count
作为控制上下文的变量, preempt_count 是 int 型，一共 32 位。通过设置该变量不同的位来设置内核中的上下文标志，包括硬中断上下文、软中断上下文、进程上下文等，通过判断该变量的值就可以判断当前程序所属的上下文状态。  

preempt_count 中 32 位的分配是这样的：TODO

如图所示，整个 preempt_count 被分为几个部分：
* 0~7位：这八位用于抢占计数，linux 内核支持进程的抢占调度，但是在很多情况下我们需要禁止抢占，每禁止一次，这个数值就加 1，在使能抢占的时候将减 1，系统支持的最大嵌套次数为 256 次。  
* 8~15位：描述软中断的标志位，因为软中断在单个 cpu 上是不会嵌套执行的，所以只需要用第 8 位就可以用来判断当前是否处于软中断的上下文中，而其它的 9~15 位用于记录关闭软中断的次数。  
* 16~19位：描述硬中断嵌套次数的，在老版本的 linux 上支持中断的嵌套，但是自从 2.6 版本之后内核就不再支持中断嵌套，所以其实只用到了一位，如果这部分为正数表示在硬件中断上下文，为 0 则表示不在。  


## 上下文的操作接口

内核提供了一系列的对上下文的设置和查询操作，需要注意的是，所有的这些对于上下文的操作都是基于 local cpu 的。  

### preempt_count 操作
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

### 判断是否处于 softirq 上下文


### enable

local_bh_enable 实现源码如下：

```
static inline void local_bh_enable(void)
{
	__local_bh_enable_ip(_THIS_IP_, SOFTIRQ_DISABLE_OFFSET);
}
```

其中 SOFTIRQ_DISABLE_OFFSET 为 softirq 在 preempt_count 变量中对应位的偏移值：

```c++
#define SOFTIRQ_DISABLE_OFFSET	(2 * SOFTIRQ_OFFSET)
#define SOFTIRQ_OFFSET	(1UL << SOFTIRQ_SHIFT)
#define SOFTIRQ_SHIFT	(PREEMPT_SHIFT + PREEMPT_BITS)
#define PREEMPT_SHIFT	0
#define PREEMPT_BITS	8
```

经过一系列兜兜转转的宏展开，



### 硬中断接口


### 公共接口
preemptible

in_interrupt

in_atomic

preempt_count() 并非只是返回 preempt_count 的值，和抢占没有直接关系。