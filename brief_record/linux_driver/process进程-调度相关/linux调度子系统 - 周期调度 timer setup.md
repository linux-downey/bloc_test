# linux调度子系统 - 周期调度 timer setup
linux 中的周期性调度器负责周期性地更新运行进程以及队列的时间信息，并检查是否需要重新调度.那么，周期性调度是如何产生的呢?

要回答这个问题其实并不会很简单，它涉及到 linux 时间子系统的实现，而且这部分的实现与具体平台有较强的相关性，本文尝试以 arm 平台为例来追溯 tick 中断的产生，对于时间子系统中调度无关的部分以及一些高级特性暂时不表.  


## 从硬件开始
不难猜到，周期性 tick 是基于定时器实现的，定时器是一种硬件设备，不同的厂商提供不同的定时设备，对于早期的单核 arm 而言，timer 通常是厂商自定义的外设，并不统一，定时器的操作以及模式支持需要参考对应的数据手册.  

进入到多核时代之后，arm 在 armv7，armv8 架构中将定时器集成到了架构设计中，参考下面的定时器硬件架构:

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/schedule/arm%E5%AE%9A%E6%97%B6%E5%99%A8%E6%A1%86%E6%9E%B6%E5%9B%BE.jpg)  

这是从 armv7-A 手册中 B8.1 section 中描述 MP core 定时器架构的图，对于 armv8 而言，定时器也是同样的框架，只是在处理器模式以及访问上存在区别.  

从框架图以及手册中提供的介绍，可以对 armv7 的 timer 框架有一个大致的了解:



* 每个 CPU 核都包含各自的 local timer，相互独立
* 系统中存在一个 always-powered 的域，这个域提供一个 system counter，所有 MP core 的定时器都是基于这个 system counter 提供的 counter 值，因此理论上所有的 local timer 都是基于同样的时间基准.  
* 为什么要强调 system counter 是 always powered，而且要独立出来，这是因为在系统运行期间某些 MP core 为了节能可能进入睡眠状态，local timer 可能也会因此被关闭，但是系统的时间戳不能丢，以便在特定的时间唤醒 CPU，而且在唤醒之后还能获得正确的时间.同时，system counter 也支持休眠模式，它的休眠不是关闭，而是降频，通常情况下该 timer 的频率是 1~50MHz，假设是以 10MHz 运行，将其降到 1MHz，那么，system counter 每次运行时 counter 不再是加1，而是加 10，这样就不会丢失时间精度. 
* system counter 的实现标准为:
    1.至少 56 bits 的宽度.
    2.频率在 1-50MHz
    3.溢出时间至少在 40 年
    4.arm 没有对精度做出特别要求，不过最低的建议值为:24小时误差不超过 10s
    5.从 0 开始计数，正常情况下每一个时钟脉冲加1，节能模式下除外. 
* system counter 可以被所有 Mp core 访问，通过总线地址映射的方式，而 local timer 由对应的 CPU core 访问，访问方式则是通过操作 CP15 协处理器
* 从硬件框架图中不难看出，每个 local timer 都支持中断的产生，中断类型为 PPI，即 CPU 核的私有中断，GIC 负责分发到指定的 CPU，这些中断都可以用来产生系统事件，就目前而言，在多核 armv7，armv8 架构中，tick 中断的产生就是通过 percpu 的 local timer 实现，当然也可以由厂商自行设计外设 timer 来产生 tick，只是在一般情况下并没有什么必要.  




## clock_event
硬件上 timer 的支持是一方面，另一方面就是软件对于硬件的抽象.  

如果光从软件的角度来看，linux 中对时间会有哪些需求:



* 来自系统计时的需求，换句话说系统需要知道现在是xx年xx月xx日xx时xx分xx秒xx纳秒。 
* 系统需要软件timer的服务。即从当前时间点开始，到xxx纳秒之后通知我做某些事情
* 对某些事件之间的时间长度进行度量

实际上，linux内核应对上面的三种需求，分别提出了clocksource，clockevent和timercounter的概念。到底底层的HW情况是怎样的，who care？只要能满足需求就足够了。你可以考虑只提供若干个free running的counter A1 A2....，然后提供另外一个能够产生中断的hw timer B1 B2....，可是分开的HW block又能得到什么好处呢？合在一起还能复用某些HW block呢。因此，在单核时代，大部分的HW timer都是free running的counter ＋ 产生中断的HW timer，来到多核时代，实际上这些block又可以分开，例如各个CPU core都有自己的local timer HW block，共享一个系统的HW counter。不过，无论硬件如何变，从软件需求的角度看看往往可以看到更本质的东西。(**原文摘自wowo科技:[Linux时间子系统之（十五）：clocksource](http://www.wowotech.net/timer_subsystem/clocksource.html) 评论区郭健的回答，自认为不能比这位前辈描述得更好，就直接拿来用了**)

在这里并不准备讨论 clocksource 和 timercounter 这两部分，但是我们需要知道内核中对于硬件 timer 的抽象包括这两个部分，以便对于 linux 的时间管理有一个大概的认识.而 clock_event 是内核中对于定时任务的抽象，也正是 tick 周期中断的生产者.  





## timer 驱动
要了解 system timer 在系统中是如何作用的，就需要从驱动入手，由于手头上没有 armv7 MP core 的硬件，本文就基于 imx8 平台(Armv8，4核cortex A53)的 timer 驱动进行分析.好在 armv7 和 armv8 的 timer 软硬件框架相差不大(主要差别在于控制访问以及处理器模式，本文不涉及)，不影响对 timer 的理解.  

对 local timer 的描述，在设备树中:

```c++
timer {
    compatible = "arm，armv8-timer";
    interrupts = <GIC_PPI 13 (GIC_CPU_MASK_SIMPLE(6) | IRQ_TYPE_LEVEL_LOW)>， /* Physical Secure */
                <GIC_PPI 14 (GIC_CPU_MASK_SIMPLE(6) | IRQ_TYPE_LEVEL_LOW)>， /* Physical Non-Secure */
                <GIC_PPI 11 (GIC_CPU_MASK_SIMPLE(6) | IRQ_TYPE_LEVEL_LOW)>， /* Virtual */
                <GIC_PPI 10 (GIC_CPU_MASK_SIMPLE(6) | IRQ_TYPE_LEVEL_LOW)>; /* Hypervisor */
    clock-frequency = <8333333>;
    interrupt-parent = <&gic>;
    arm，no-tick-in-suspend;
};
```
因为 timer 是集成在 armv8 体系架构中的，因此 compatible 属性为 "arm，armv8-timer"，熟悉驱动程序的朋友都知道，这是一个匹配字段，用来匹配同名的 driver.  

interrupts 定义了 timer 的中断属性，armv8 中包含四中类型的中断，分别针对不同的处理器模式，同时提供了对应的 PPI 中断号以及中断状态.以及设置 interrupt-parent 为 gic，这是系统的中断控制器，表示当前 timer 输出的中断信号将发送给 gic，由 gic 分发给对应的 CPU.  

clock-frequency 为 timer 频率，arm，no-tick-in-suspend 这个属性表示:当 CPU 进入 suspend 状态时，当前 timer 也会停止，该特性通常服务于内核调度策略中 NO_HZ 的配置，空闲时停掉 timer 将节省功耗.   




## 驱动源码
通过设备树的 compatible 属性在内核源码中全局搜索，就可以找到对应的驱动代码位于:drivers/clocksourse/arm_arch_timer.c 中:

```c++
TIMER_OF_DECLARE(armv7_arch_timer， "arm，armv7-timer"， arch_timer_of_init);
TIMER_OF_DECLARE(armv8_arch_timer， "arm，armv8-timer"， arch_timer_of_init);
```
可以看到，armv7 和 armv8 是共用一份 timer 驱动代码的.其核心的实现在 arch_timer_of_init 中:

```c++
static int __init arch_timer_of_init(struct device_node *np)
{
	int i， ret;
	u32 rate;

    // 设置 ARCH_TIMER_TYPE_CP15 标志位，防止重复地初始化 local timer.
	if (arch_timers_present & ARCH_TIMER_TYPE_CP15) {
		return 0;
	}
	arch_timers_present |= ARCH_TIMER_TYPE_CP15;

    // 解析设备树节点，获取对应的 ppi number，保存在 arch_timer_ppi 中后续使用.如何使用?当然是用来注册中断
	for (i = ARCH_TIMER_PHYS_SECURE_PPI; i < ARCH_TIMER_MAX_TIMER_PPI; i++)
		arch_timer_ppi[i] = irq_of_parse_and_map(np， i);];

    // 硬件上支持多种类型的 local timer，全部列出在设备树中，内核需要根据实际的应用情况选择其中一个 timer 进行初始化，作为 sched timer，选择的依据为:
    //   * 如果设备树指定了 arm，cpu-registers-not-fw-configured 属性，使用 secure timer
    //   * 如果 kernel 当前的处理器模式为 hyp mode，使用 hyp 类型的 timer
    //   * 如果不支持 hyp mode 在内核中为 Invalable，且提供了 virtual timer，则使用 virtual timer
    //   * 如果内核定义了 CONFIG_ARM64，也就是为 arm64 架构，使用 Non secure timer
    //   * 否则使用 secure timer
	if (IS_ENABLED(CONFIG_ARM) &&
	    of_property_read_bool(np， "arm，cpu-registers-not-fw-configured"))
		arch_timer_uses_ppi = ARCH_TIMER_PHYS_SECURE_PPI;
	else
		arch_timer_uses_ppi = arch_timer_select_ppi();

    // 将 timer 实际注册到系统中，见下文
	ret = arch_timer_register();
	if (ret)
		return ret;

    // 公共的 timer 初始化部分，见下文
	return arch_timer_common_init();  
}
```

local timer 初始化函数中核心的部分在于 arch_timer_register 和 arch_timer_common_init 两个接口，我们先来看 arch_timer_register:


arch_timer_of_init->arch_timer_register
```c++
static int __init arch_timer_register(void)
{
	int err;
	int ppi;

    // 分配 percpu 的 struct clock_event_device 结构实体，timer 是 local 的，因此对应的 data 部分也是 percpu 的，clock_event_device 用来描述一个产生 clock_event 的设备，clock_event 也就是定时中断.  
	arch_timer_evt = alloc_percpu(struct clock_event_device);

    // 见下文
	ppi = arch_timer_ppi[arch_timer_uses_ppi];
	switch (arch_timer_uses_ppi) {
	case ARCH_TIMER_VIRT_PPI:
		...
	case ARCH_TIMER_PHYS_SECURE_PPI:
	case ARCH_TIMER_PHYS_NONSECURE_PPI:
		err = request_percpu_irq(ppi， arch_timer_handler_phys，
					 "arch_timer"， arch_timer_evt);
		...
		break;
	case ARCH_TIMER_HYP_PPI:
		...
	default:

	}

	err = cpuhp_setup_state(CPUHP_AP_ARM_ARCH_TIMER_STARTING，
				"clockevents/arm/arch_timer:starting"，
				arch_timer_starting_cpu， arch_timer_dying_cpu);
}
```

arch_timer_register 函数会根据前面对 local timer 的选择，注册对应的 ppi 中断，中断 handler 为 arch_timer_handler_phys，附属的数据为 arch_timer_evt，当 local timer 配置完成，产生中断时，就会执行该中断 handler.   

该中断对应的 handler 函数为: 

```c++
Hardirq->arch_timer_handler_phys:

static irqreturn_t arch_timer_handler_phys(int irq， void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	return timer_handler(ARCH_TIMER_PHYS_ACCESS， evt);
}

Hardirq->arch_timer_handler_phys->timer_handler:

static __always_inline irqreturn_t timer_handler(const int access，
					struct clock_event_device *evt)
{
	unsigned long ctrl;

	ctrl = arch_timer_reg_read(access， ARCH_TIMER_REG_CTRL， evt);
	if (ctrl & ARCH_TIMER_CTRL_IT_STAT) {
		ctrl |= ARCH_TIMER_CTRL_IT_MASK;
		arch_timer_reg_write(access， ARCH_TIMER_REG_CTRL， ctrl， evt);
		evt->event_handler(evt);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

```
在 timer 的 ppi 中断中，做了两件事:

1.获取 percpu 的 clock_event_device 结构，调用对应的回调函数 evt->event_handler()，传入的参数也正是该 event 结构
2.读取并设置 timer 的状态， armv8 中 local timer 并没有在总线上，而是通过 CP15 协处理器访问，因此需要使用汇编指令 mrs/msr 进行操作.  

这里知道了 timer 中断的由来，但是在之前的代码中只见到的了 clock_event_device 的 alloc，并没有进行相应的设置和注册，同时，timer 的相关参数也没有进行配置，前面的代码并没有涉及到 timer 如何触发，以及对应的周期是多少.  

继续看到 arch_timer_register 中的 cpuhp_setup_state 函数，从名称可以看出，cpuhp 表示 cpu hotplug，热插拔相关的，并注册了两个函数:arch_timer_starting_cpu 和 arch_timer_dying_cpu，分别对应 cpu 的接入和移除时调用的函数，而系统刚启动实际上也可以被理解为 CPU 插入到系统中，因此对应的 arch_timer_starting_cpu 函数被执行.  

arch_timer_starting_cpu 调用了 \_\_arch_timer_setup，在 \_\_arch_timer_setup 中可以看到 clock_event_device 的初始化:

```c++
arch_timer_register->arch_timer_starting_cpu->__arch_timer_setup:

static void __arch_timer_setup(unsigned type，struct clock_event_device *clk)
{
    ...
    clk->name = "arch_sys_timer";
    clk->rating = 450;
    clk->cpumask = cpumask_of(smp_processor_id());
    clk->irq = arch_timer_ppi[arch_timer_uses_ppi];
    switch (arch_timer_uses_ppi) {
        ...
    case ARCH_TIMER_PHYS_SECURE_PPI:
    case ARCH_TIMER_PHYS_NONSECURE_PPI:
    case ARCH_TIMER_HYP_PPI:
        clk->set_state_shutdown = arch_timer_shutdown_phys;
        clk->set_state_oneshot_stopped = arch_timer_shutdown_phys;
        clk->set_next_event = arch_timer_set_next_event_phys;
        break;
    ...

    clockevents_config_and_register(clk， arch_timer_rate， 0xf， 0x7fffffff);
}
```
在 __arch_timer_setup 中，对 clock_event_device 初始化了 name，rate，cpumask，irq 等成员，同时设置了 timer 相关的回调函数，这些回调函数确定了 timer 的设置，包括启停执行的动作，以及如何设置下一次 timer 事件.  

函数最后，调用 clockevents_config_and_register，从名称不难看出，这个函数就是对 clock_event_device 进行配置和注册，clock_event_device 的配置和注册过程并不简单，其中涉及到几个方面:



* tick device 选择 clock_event，tick device 也就是用来产生 tick 的设备，在前文中我们都是直接假定了 local device 被作为 tick 中断的生产者，尽管 arm 中大多是这么设计的，但是实际情况是:系统中可以注册多个 clock_event_device，系统会选定一个合适的作为产生 tick 中断的设备，而并不一定是 local timer.
* broadcast 的处理，在硬件框架部分有提到， local timer 可以配置在 CPU suspend 的同时被关闭，以节省功耗，这时候需要考虑的问题是 CPU 在什么时候唤醒呢?CPU 想配置成在下一个逻辑定时器到期时唤醒，毕竟睡眠只是暂时的，但是 local timer 被关闭，所有的逻辑定时器都已经失效，自然无法唤醒 CPU.别忘了，还有一个 global 的 system counter，解决方案就是使用 system counter 作为一个广播设备，就可以执行之前 CPU 的工作，可能是唤醒 CPU，也可能是其它.当然，前面说的 system counter 和 local timer 只是 arm 架构中的 timer 框架，对于其它的体系架构也可能是其它的处理方式，但是其基本原理是差不多的. 
* 定时器使用 oneshot 还是 periodic 模式，oneshot 表示一次配置只产生一次中断，而 periodic 则是一次配置循环产生中断，对于高精度要求的定时器，通常是 oneshot 模式，毕竟 periodic 运行时容易产生误差的累积，尽管 oneshot 的操作要麻烦一些，毕竟它的可操作性要更高.
* NO_HZ 的配置，当 CPU 运行 idle 进程或者只有一个进程就绪时，可配置 NO_HZ，即停掉 CPU 的 tick 中断. 
* 高精度 timer，暂时不懂...

一个 clock_event_device 的配置和注册涉及到时间子系统相关的配置比较多，但是回过头来说，其实我们的目的就只是需要找到 clock_event_device 的 event_handler 成员，因为它会在 local timer 中断中被调用，如果该 local timer 确实是作为 tick 中断的生产者，那么该 event_handler 中最终肯定会调用到 scheduler_tick() 函数，也就是周期性调度函数.    



经过层层挖掘，找到了 event_handler 的设置，它的函数调用路径为:
arch_timer_register
    arch_timer_starting_cpu 
        __arch_timer_setup
            clockevents_config_and_register
                clockevents_register_device
                    tick_check_new_device
                        tick_setup_device
                            tick_setup_periodic
                                tick_set_periodic_handler
                                    (struct clock_event_device *)dev->event_handler = tick_handle_periodic;
                                tick_setup_oneshot

在函数 tick_set_periodic_handler 和 tick_setup_oneshot 中都会设置 event_handler，以 tick_set_periodic_handler 为例，最终设置为 tick_handle_periodic，这个函数就是在 local timer 中断中将被周期调用的接口，但它并不是直接的 scheduler_tick 函数，至于原因，我们继续往下看. 



### 周期调度函数
在阅读调度相关文章或者代码的时候，不知道你会不会有这样的疑问:对于多核系统而言，其调度行为是各自独立的，但是对于某些系统共有的资源，比如 jiffies 以及系统时间的更新，是如何更新的呢?  

接着看 local timer 的中断函数 tick_handle_periodic，该函数调用了 tick_periodic:


```c++
Hardirq->arch_timer_handler_phys->timer_handler->tick_handle_periodic->tick_periodic:
static void tick_periodic(int cpu)
{
    // tick_do_timer_cpu 指定特定更新的 CPU，默认为 TICK_DO_TIMER_BOOT
    if (tick_do_timer_cpu == cpu) {
        // 更新 jiffies 的 seqlock，seqlock 是写优先的锁
        write_seqlock(&jiffies_lock);
        ...
        // 更新 jiffies_64 
        do_timer(1);
        write_sequnlock(&jiffies_lock);
        // 更新墙上时间
        update_wall_time();
    }

    update_process_times(user_mode(get_irq_regs()));
    ...
}
```
在 tick_periodic 中，实际上并不是每个 CPU 的 timer 中断都更新系统的全局时间参数，而是选取其中一个 CPU 来更新，这个 CPU 的 num 由 TICK_DO_TIMER_BOOT 指定，通常是 0 号 CPU。  

需要注意的是，在更新 jiffies 之前，需要使用顺序锁进行保护，顺序锁是一种写优先锁，因为系统时间的更新需要尽量保证其实时性，因此这里最好是使用顺序锁，让写优先。  

在 do_timer 中，对 jiffies_64 进行更新，并没有看到 jiffies 的更新，这是因为，内核在链接的时候做了特殊处理，将 jiffies 和 jiffies_64 使用了同样的地址空间，至于 jiffies 占 jiffies_64 的高 32 bit 还是低 32 bit，取决于机器的大小端。  

update_wall_time 表示更新墙上时间，也就是我们通常使用 date 命令显示的时间，当然，只是更新底层的数据结构 clock_source 和 timerkeeper，上层应用获取的墙上时间就是请求这些时间数据。  

最后，tick_periodic 调用 update_process_times，注意，这里已经跳出了 if (tick_do_timer_cpu == cpu) 的判断，回到的 percpu 的 timer 操作。  

在 update_process_times 中，终于看到了 scheduler_tick() 函数，解决了刚开始提出的那个问题：tick 中断是如何产生的？  



## sched_clock 实现
在周期性调度器那一章(TODO)，我们频繁地提到一个接口：sched_clock()，这个接口被用于获取当前 CPU 上的时间戳，那它是如何实现的呢？  

让我们回到 local timer 驱动中的 arch_timer_of_init 函数，上面我们一直在讨论 arch_timer_register，以找到 tick 产生的证据。  

实际上还有一部分，就是 arch_timer_common_init().  

```c++
arch_timer_of_init->arch_timer_common_init:

static int __init arch_timer_common_init(void)
{
    ...
	arch_counter_register(arch_timers_present);
	...
}

arch_timer_of_init->arch_timer_common_init->arch_counter_register:
static void __init arch_counter_register(unsigned type)
{
	u64 start_count;

	if (type & ARCH_TIMER_TYPE_CP15) {
		if (IS_ENABLED(CONFIG_ARM64) ||
		    arch_timer_uses_ppi == ARCH_TIMER_VIRT_PPI)
			arch_timer_read_counter = arch_counter_get_cntvct;
		else
			arch_timer_read_counter = arch_counter_get_cntpct;

		clocksource_counter.archdata.vdso_direct = vdso_default;
	} else {
		arch_timer_read_counter = arch_counter_get_cntvct_mem;
	}

	sched_clock_register(arch_timer_read_counter， 56， arch_timer_rate);
}
```
在 arch_timer_common_init 中调用了 arch_counter_register，从名称可以看出，这是对 system counter 的注册，紧接着在 arch_counter_register 将一个读取当前 system counter 的回调函数传递给 sched_clock_register 注册到系统中，至于这个读取当前 system counter 的回调函数是哪一个，取决于 system counter 的访问方式和使用哪个 local timer，不同的 read 回调函数实现和访问方式不一样，但返回结果都是 system counter 的当前值。  

那么，sched_clock_register 注册 system counter 又是做了什么呢？

```c++
void __init
sched_clock_register(u64 (*read)(void)， int bits， unsigned long rate)
{
    ...
    struct clock_read_data rd;
    rd.read_sched_clock	= read;
	...
	update_clock_read_data(&rd);
}
```

在 sched_clock_register 中，填充一个 struct clock_read_data 结构 rd，将传入的读取 system counter 的回调函数赋值给 rd.read_sched_clock，并更新到系统。   

而在调度器频繁使用的 sched_clock() 中，正是调用了该回调函数以获取 system counter 的时间：

```c++

unsigned long long notrace sched_clock(void)
{
    ...
		cyc = (rd->read_sched_clock() - rd->epoch_cyc) &
		      rd->sched_clock_mask;
    ...
}
```

至此，sched_clock 实现的真相也就水落石出。  





