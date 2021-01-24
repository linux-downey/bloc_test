# linux 下半部机制-softirq 的实现原理

在上一章中我们了解了 softirq 的使用方法，按照我们一贯的风格，由表及里。我们来看看 softirq 的实现原理,软中断的实现代码在。   


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

当开发者需要执行自己的软中断时，调用 raise_softirq(nr) 函数，该函数就会把需要执行的 irq 标志位置位,它的实现是这样的：

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
* 关本地中断：很多朋友会有疑问了，不是说软中断可以被硬中断抢占执行吗，为什么这里关中断了？需要注意的是，raise_softirq 函数并不是真正执行软中断，只是设置标志位来表示这个软中断处于 pending 状态，表示可以在软中断调度点被调度执行了。  
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

smpboot_register_percpu_thread 这个函数将会创建 percpu 类型的内核线程，也就是每个 cpu 都对应一个线程，值得注意的是，这个函数还支持热插拔的 cpu，可以动态地根据 cpu 的增减来更新这些内核线程，在这里并不准备深入解析 smpboot_register_percpu_thread 的源码实现，有兴趣的朋友可以自己研究一下。  

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

该函数将在下文中详细讨论。  


### softirq 的调度点
除了内核线程执行 __do_softirq() 函数之外，在上一章中提到了 softirq 的两个调用点：

#### irq_exit:
也就是硬中断退出的时候，其源码实现为：

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
在 irq_exit 中调用了 invoke_softirq() 函数，值得注意的是，在调用 invoke_softirq() 之前先要判断当前上下文是否处在中断(包括硬中断、软中断、tasklet)上下文中，只有不处于中断上下文的时候才会执行 invoke_softirq();   

看到这部分代码很容易产生一个疑惑：硬中断的退出函数 irq_exit() 不应该还是处在硬中断上下文中吗？这样的话 invoke_softirq() 永远也不会被调用了。   

事实上，程序处在哪个上下文并不是由执行函数来划定的，而是由一个标志位来确定，这个标志位就是 current_thread_info()->preempt_count，利用该变量的某些位用来指示上下文状态，包括硬中断、软中断、内核抢占使能标志等等，关于这个标志位将会另开一个专题讲解。  

所以，在判断 in_interrupt() 之前调用了 preempt_count_sub(HARDIRQ_OFFSET)，该函数设置 preempt_count，表示当前退出中断上下文，所以 !in_inerrupt() 函数通常为真。   

那为什么需要判断 !in_inerrupt() 这个条件呢？那是因为在某些时候，软中断被硬件中断抢占执行，硬中断退出时，尽管退出了硬中断上下文，但是仍处于软中断上下文中，这时候不应该嵌套执行软中断，因为硬中断函数返回之后会继续执行被打断的软中断。   

而 local_softirq_pending() 用来检查 irq_stat 标志位中是否有软中断就绪标志位被置位。  

invoke_softirq() 的实现是这样的：

```c++
static inline void invoke_softirq(void)
{
	if (ksoftirqd_running(local_softirq_pending()))
		return;

	if (!force_irqthreads) {
		...
		__do_softirq();
#endif
	} else {
		wakeup_softirqd();
	}
}
```
在 invoke_softirq 中，首先判断负责处理 softirq 的内核线程是否正在运行，如果是，就返回，让内核线程继续处理。  

然后，判断 force_irqthreads 变量，从变量名可以看出，这个变量表示是否强制使用内核线程来处理 softirq，如果不是，就调用 __do_softirq ，如果是，就唤醒内核线程，让线程来处理，从上文的分析可知，内核线程最终调用 __do_softirq 函数。   

同样的，我们着急分析 __do_softirq 函数，继续看 softirq 的下一个调用点。   


#### local_bh_enable
该函数通常表示重新使能下半部的时候，和其他同步机制屏蔽使能作用域是一样的，它只作用于 local cpu，它的源码如下：

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
刨去一些检查和不必要的部分，这个函数同样会判断是否处于中断上下文， 的主体就是调用 do_softirq(),

```c++
asmlinkage __visible void do_softirq(void)
{
	...
	if (pending && !ksoftirqd_running(pending))
		do_softirq_own_stack();
	...
}

```
do_softirq() 同样判断内核线程是否在运行，然后调用 do_softirq_own_stack()，这个函数表示使用 softirq 自己的栈，默认情况下，无论是硬中断还是软中断，是没有自己的栈的，而是借用了被抢占进程的栈，irq 栈和内核栈是否分离这取决于硬件的设计和软件实现，总之，do_softirq_own_stack() 函数最终会调用到 __do_softirq() 函数。  


### __do_softirq 函数执行
分析完了 __do_softirq 的几个被调用的节点，再来看它的实现：

```c++
asmlinkage __visible void __softirq_entry __do_softirq(void)
{
	struct softirq_action *h;
	__u32 pending;
	int softirq_bit;
	...
	//取出全局 softirq 标志位
	pending = local_softirq_pending();
restart：
	//获取数组向量的地址
	h = softirq_vec;
	//逐位地判断 softirq 标志位是否被置位，被置位的需要执行
	while ((softirq_bit = ffs(pending))) {
		unsigned int vec_nr;
		int prev_count;

		h += softirq_bit - 1;
		vec_nr = h - softirq_vec;
		//执行 softirq 的工作函数
		h->action(h);
		h++;
		pending >>= softirq_bit;
	}
	...
	//判断是否需要唤醒内核线程协助处理
	pending = local_softirq_pending();
	if (pending) {
		if (time_before(jiffies, end) && !need_resched() &&
		    --max_restart)
			goto restart;

		wakeup_softirqd();
	}
	...
}
```

其实从函数命名就可以看出这个函数是用来处理软中断的，它的执行流程就是：通过扫描 irq_stat[this_cpu].__softirq_pending 变量的每一位来确定有哪些软中断需要被执行，如果对应标志位被置位，就调用 softirq_vec 向量数组中对应的 action 函数以执行 softirq，不知道你还记不记得，这个向量数组是在 open_softirq 时设置的。   

在执行完所有应该执行的 softirq 之后，再次判断是否有 __softirq_pending 标志位被置位，也就是说在此次 softirq 执行的过程中是否又产生了 softirq 工作，如果有，将会判断三个条件：
* softirq 的处理时间比较短，小于 MAX_SOFTIRQ_TIME，这个时间间隔的值设置为宏 MAX_SOFTIRQ_TIME，在 4.9 中默认为 msecs_to_jiffies(2)，也就是 2ms，不过需要将这 2ms 转换为 jiffies，实际上，这取决于系统内的 HZ 值，如果 HZ 不大于 500，最终的延时值就是一个 jiffies，虽然看起来设置为 2ms，实际上通常不是。
* 没有高优先级的任务需要抢占运行
* 重新调用 softirq 的次数小于 10

同时满足这三个条件，将会执行 goto restart，跳转到当前函数的前面重新执行。   

否则，则表明系统响应 softirq 比较繁忙，需要调用 wakeup_softirqd 唤醒内核线程来处理，以免过多地执行 softirq 导致进程过多的延迟。   
















