# linux 下半部机制-softirq 的实现原理

在上一章中我们了解了 softirq 的使用方法，按照我们一贯的风格，由表及里。我们来看看 softirq 的实现原理。   


## softirq 初始化







### softirq 向量数组的初始化

在上一章中，我们讲到想要添加一个软中断需要在枚举列表中添加一项，枚举结构的最后一项 NR_SOFTIRQS 就是软中断的数量，内核为这些软中断创建了一个全局的数组

```c++
static struct softirq_action softirq_vec[NR_SOFTIRQS] __cacheline_aligned_in_smp;

const char * const softirq_to_name[NR_SOFTIRQS] = {
	"HI", "TIMER", "NET_TX", "NET_RX", "BLOCK", "IRQ_POLL",
	"TASKLET", "SCHED", "HRTIMER", "RCU"
};
```

而添加软中断的函数为 open_softirq，其实现为：

```c++
void open_softirq(int nr, void (*action)(struct softirq_action *))
{
	softirq_vec[nr].action = action;
}
```
将用户传入的 action 函数复制到 softirq_vec[nr].action 中。所以 softirq_vec 相当于一个向量数组，根据数组下标就可以找到对应的 action 回调函数。所以，用户在 raise_softirq 的时候只需要传入 nr，就可以使用 nr 作为数组下标找到对应的 action 函数了。  


### 内核如何知道是否有 softirq 需要执行

在内核中，同样定义了一个全局数组，这个数组是标志位的集合，它的定义是这样的：

```c++
typedef struct {
    //软中断标志位
	unsigned int __softirq_pending;
#ifdef CONFIG_SMP
    //IPI 中断
	unsigned int ipi_irqs[NR_IPI];
#endif
} ____cacheline_aligned irq_cpustat_t;

irq_cpustat_t irq_stat[NR_CPUS] ____cacheline_aligned;
```

数组成员的数量为当前系统中 CPU 的数量，每个数组元素包含两个成员：
* 软中断标志位，__softirq_pending，每一个在枚举结构中定义的变量软中断都对应该变量中的一位，因为这个变量是 32 位的，所以系统目前最多支持 32 个软中断。
* ipi_irqs ：这是 IPI 类型的软中断，属于 cpu 与 cpu 之间的中断，在 SMP 系统中，cpu 可以向另一个 cpu 发送中断信号，这个中断信号并非硬件上的，所以也属于软中断的一种，IPI 类型的中断不在这里详细讨论。  

当开发者需要执行自己的软中断时，调用 raise_softirq(nr) 函数，它的实现是这样的：

```c++
void raise_softirq(unsigned int nr)
{
	unsigned long flags;

	local_irq_save(flags);
	raise_softirq_irqoff(nr);
	local_irq_restore(flags);
}

inline void raise_softirq_irqoff(unsigned int nr)
{
    //设置标志位
	__raise_softirq_irqoff(nr);

	//如果不是在中断环境下调用，使用内核线程来处理
	if (!in_interrupt())
		wakeup_softirqd();
}
```

raise_softirq 的实现主要是三个部分：
* 关本地中断：很多朋友会有疑问了，不是说软中断可以被硬中断抢占执行吗，为什么这里关中断了？需要注意的是，raise_softirq 函数并不是真正执行软中断，只是设置标志位来表示这个软中断处于 pending 状态，可以在软中断调度点被调度执行了。  
* 关闭中断后，调用 raise_softirq_irqoff，raise_softirq_irqoff 中调用 __raise_softirq_irqoff(nr) 函数，根据跟踪代码可以发现，这个函数其实就是执行了下面这一段代码：

    ```
    irq_stat[smp_processor_id()].__softirq_pending |= 1<<nr
    ```

    也就是将当前 cpu 的 __softirq_pending 对应的软中断位置1.

* raise_softirq_irqoff 的 in_interrupt 函数用于判断当前是否处于中断环境下(包含硬中断、softirq、tasklet)，是则返回真，通常情况下，raise_softirq 会在硬中断处理程序中调用，也有一些特殊情况下它的调用不在中断环境下，这个时候需要唤醒内核线程来处理该 softirq 以保证能快速地处理 softirq，因为 softirq 的调度时机为中断退出和重新使能 softirq 的时候，如果不主动唤醒内核线程处理，将会导致该 softirq 被延迟。  




### softirq 内核线程的初始化
spawn_ksoftirqd 这个函数使用 early_initcall 进行修饰，在内核启动阶段会被自动执行，主要是为当前的 softirq 创建内核线程，在特殊的时候作为载体执行 softirq：

```c++

static struct smp_hotplug_thread softirq_threads = {
	.store			= &ksoftirqd,
	.thread_should_run	= ksoftirqd_should_run,
	.thread_fn		= run_ksoftirqd,
	.thread_comm		= "ksoftirqd/%u",
};

DEFINE_PER_CPU(struct task_struct *, ksoftirqd);

static __init int spawn_ksoftirqd(void)
{
	...
	smpboot_register_percpu_thread(&softirq_threads);
    ...
	return 0;
}
```

在这里并不准备深入解析 smpboot_register_percpu_thread 的源码实现，smpboot_register_percpu_thread 这个函数将会创建 percpu 类型的内核线程，也就是每个 cpu 都对应一个线程，值得注意的是，这个函数还支持热插拔的 cpu，可以动态地根据 cpu 的增减来更新这些内核线程。  

传入的参数为 softirq_threads，主要是两个成员：ksoftirqd_should_run 和 run_ksoftirqd。

ksoftirqd_should_run 用于判断是否需要执行 run_ksoftirqd 这个回调函数：

```
static int ksoftirqd_should_run(unsigned int cpu)
{
	return local_softirq_pending();
}
```

local_softirq_pending 就是检查标志位 irq_stat[smp_processor_id()].__softirq_pending 相应的软中断位是否被置位，如果被置位，则返回真，同时也表示内核中调用了 raise_softirq 。  

run_ksoftirqd 是内核线程的执行函数，在 local_softirq_pending 返回真的情况下会执行当前函数，执行完之后再次判断 local_softirq_pending，此时一般没有 softirq 需要处理，将会进入睡眠，唤醒线程需要使用 wakeup_softirqd() 接口。  

run_ksoftirqd 的实现是这样的：

```c++
static void run_ksoftirqd(unsigned int cpu)
{
	local_irq_disable();
	if (local_softirq_pending()) {
		__do_softirq();
		local_irq_enable();
		cond_resched_rcu_qs();
		return;
	}
	local_irq_enable();
}
```
在该函数中调用了 __do_softirq() 函数，从名字可以看出这个函数负责处理 softirq。  


### softirq 的执行
除了内核线程执行 __do_softirq() 函数之外，在上一章中提到了 softirq 的两个调用点：
* irq_exit:也就是硬中断退出的时候
    
    ```c++
    void irq_exit(void)
    {
        ...
        preempt_count_sub(HARDIRQ_OFFSET);
        if (!in_interrupt() && local_softirq_pending())
            invoke_softirq();
        ...
    }
    ```

* local_bh_enable：重新使能下半部的时候

    ```c++
    void __local_bh_enable_ip(unsigned long ip, unsigned int cnt)
    {
        ...
        if (unlikely(!in_interrupt() && local_softirq_pending())) {
            do_softirq();
        }
        ...
    }
```

















