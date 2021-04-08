# linux中断子系统 - linux 中断处理流程

中断发生的时候,硬件上直接执行跳转,跳转到中断向量表处执行中断代码,进入到 irq mode,紧接着又跳转到 svc 模式,在该模式下进行真正的中断处理行为,这些模式跳转以及上下文现场的处理都是晦涩难懂的汇编代码,做好相关的准备工作之后,执行 handle_arch_irq 函数,进入到 C 语言的世界. 

(注:本章节基于支持 [GICv2 标准的中断控制器](https://zhuanlan.zhihu.com/p/363129733)的 arm 平台进行讨论,后续不再强调)

## handle_arch_irq

handle_arch_irq 实际上是一个函数指针,看到 arch 字样就可以猜到,这是一个平台相关的函数,对于不同的平台,这个函数指针可以赋予不同的值,在支持 GICv2 的系统中,这个函数指针是在 GIC 驱动初始化的时候被设置的:

```
static int __init __gic_init_bases(struct gic_chip_data *gic,
				   int irq_start,
				   struct fwnode_handle *handle)
{
    ...
    set_handle_irq(gic_handle_irq);
    ...
}
```

set_handle_irq 设置 handle_arch_irq 等于 gic_handle_irq,也就是在中断发生之后,中断处理流程将会调用 gic_handle_irq 函数. 



## gic_handle_irq

先上代码:

```c++
static void __exception_irq_entry gic_handle_irq(struct pt_regs *regs)
{
	u32 irqstat, irqnr;
	struct gic_chip_data *gic = &gic_data[0];
	void __iomem *cpu_base = gic_data_cpu_base(gic);

	do {
		irqstat = readl_relaxed(cpu_base + GIC_CPU_INTACK);
		irqnr = irqstat & GICC_IAR_INT_ID_MASK;

		if (likely(irqnr > 15 && irqnr < 1020)) {
			handle_domain_irq(gic->domain, irqnr, regs);  ..............1
			continue;
		}
		if (irqnr < 16) {
			writel_relaxed(irqstat, cpu_base + GIC_CPU_EOI);
			...
			handle_IPI(irqnr, regs);                     ...............2
			continue;
		}
		break;
	} while (1);            
}
```



在 [arm GIC介绍](https://zhuanlan.zhihu.com/p/363129733)中了解到,GIC 支持三种中断:SGI,PPI,SPI.

 SGI 是软件触发的中断,通常用作 CPU 之间的通信,软件触发 SGI 时需要指定目标 CPU. 

PPI 是硬件触发的,不过是 CPU 独立的,最典型的例子就是每个 CPU 核的 tick timer,而 SPI 就是我们最常见的外部中断.  

内核中,针对 SGI 和 PPI/SPI 的处理是分开的,毕竟 SGI 中断只涉及到软件处理,处理起来相对简单,如果将 SGI 的处理也加入到 SPI 的处理框架中,会影响 SGI 的处理效率.

而对于 SPI 而言,需要考虑 GIC 的级联问题,当系统中存在多个 GIC 时,需要递归地进行处理.因此调用 handle_domain_irq,而 PPI 和 SPI 共用了处理部分.实际上在我看来,级联的子 GIC 不大可能会连接 PPI 中断,毕竟 percpu 的 bank 中断放在 secondary GIC 中看起来是很奇怪的.奇怪归奇怪,实现上是没问题的,因此 PPI 中断的处理放到 SPI 一起也是没太多问题.  

注1:在 arm linux 中,CPU 上使用的中断引脚只有 irq line,因此发生中断的时候,CPU需要知道中断源,中断源的信息记录在 GIC 的 GIC_CPU_INTACK 寄存器中,包括 CPU ID 和 interrupt ID,只有在 SGI 中断中 CPU ID 才是有意义的,这是 CPU interface 寄存器,也就是 percpu 的,可以通过 CPU ID num 获取当前 SGI 中断是由谁发送的. 

对于 PPI/SPI 中断,CPU ID 没有意义,因此只需要读取  GIC_CPU_INTACK 寄存器的低 10 位,获取 GIC 上发生中断的 hwirq, 如果 hwirq 大于 15 同时小于 1020,说明这是个外部的硬件中断(SPI/PPI),小于 16 表示这是 IPI 中断,需要区分处理.

注意到,发生中断时中断号的读取以及处理是在一个死循环中执行的，这种处理方式可以确保假设中断处理过程中产生额外的中断，可以直接处理，而不用重新跳转到中断向量表，当从 GIC_CPU_INTACK 读取到的中断号大于等于 1020 的时候，才会退出中断，GICv2 中通常使用 1023 表示无效的中断号。

为了讲解方便，将其它一些无关的处理省略了，包括 EOI 模式的处理，关于 EOI 模式可以参考[arm GIC介绍](https://zhuanlan.zhihu.com/p/363129733)。

## 外设中断的处理

PPI 和 SPI 都属于外设中断，对应的处理函数为 handle_domain_irq，传入的参数为当前 GIC 所属的 domain，hwirq 以及 pt_regs(保存的断点信息地址)。 

从 [arm GIC介绍](https://zhuanlan.zhihu.com/p/363129733) 可以了解到，内核中对系统中支持的每个中断都有一个编号 irq，通常这个 irq 并不对应 GIC 的 interrupt ID，而是存在一个映射关系，这个映射关于由 domain 保存，因此，需要通过 GIC domain 获取到 hwirq 对应内核中使用的逻辑 irq。 

handle_domain_irq 会间接调用到 __handle_domain_irq 函数，源码如下：

```c++
int __handle_domain_irq(struct irq_domain *domain, unsigned int hwirq,
			bool lookup, struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);
	unsigned int irq = hwirq;
	int ret = 0;

	irq_enter();                          .....................................1

	if (lookup)
		irq = irq_find_mapping(domain, hwirq);.................................2

		generic_handle_irq(irq);          .....................................3
	

	irq_exit();                          .....................................4
	set_irq_regs(old_regs);
	return ret;
}
```



注1：irq_enter 标示着内核正在中断处理的阶段，irq_enter 的源码是这样的：

```c++
void irq_enter(void)
{
	...
	if (is_idle_task(current) && !in_interrupt()) {        ........................a
		local_bh_disable();
		tick_irq_enter();
		_local_bh_enable();
	}

	__irq_enter();                                       .........................b
}
```



注b：先讲 __irq_enter，这个函数主要执行的操作就是设置进入中断的标志，就是将 preempt count 中 hardirq 位加 1(关于 preempt count 可以参考这篇文章TODO)，也就表示当前系统进入了用户级别的中断处理状态，有点奇怪的是 preemt count 的中断标志位使用了 4 bits，因为不支持中断嵌套，实际上 1bit 就行了，可能是历史遗留原因吧。

严格来说，发生中断向量的跳转之后，就表示已经进入到中断处理状态，中断标志的设置应该在这时候就被设置，实际上没必要，标示系统的中断状态的目的在于让驱动或者内核开发者确定某个函数是否在中断环境下执行，而这些函数的调用时机在自定义的中断处理回调函数中，只需要在这之前设置完中断状态即可。

因此，可以说 preempt count 的中断状态位被设置，系统当前处于中断处理状态，但是反之并不亦然。

注a：第一部分判断当前 CPU 是否正在执行 IDLE 进程，同时判断 in_interrupt 是否为 0，如果满足条件，执行一些额外的操作。  

实际上， in_interrupt() 这个函数从名称上是有点歧义的，看起来像是判断当前系统是否处于中断状态，实际上不是，除了处于中断状态，如果系统处于 softirq、nmi 中断处理时，in_interrupt 也返回真，而真正单独判断是否处于中断状态的接口是 in_irq。 

因为 __irq_enter 在 当前代码之后运行，也就是此时还没有设置 preemt count 中的中断位，!in_interrupt() 的返回通常为真，但是如果当前中断是抢占了 softirq 执行或者发生中断嵌套的情况，!in_interrupt() 就返回 false，当然，自从 2.6 版本内核已经不再支持中断嵌套了，如果哪个不守规矩的开发者在中断处理程序中将中断打开，就有可能有中断嵌套的发生。 

再回过头来看，如果两个条件都满足，将会发生什么？通过内核中的代码注释可以了解到这一部分是为了防止无意义的 softirqd 线程的唤醒，单独看这一部分代码很难看懂，只能去翻这部分代码对应的 git commit，再结合 softirq 的相关知识点，半理解半猜地得出一个并不那么确定的结论：

首先，注释说为了防止无意义的 softirqd 的唤醒，那我们就来看看 softirqd 会在什么时候被唤醒，从原理上来说，内核中进程才是主体，实现主要的功能，而中断和 softirq 以及其它下半部机制作为通知机制或者处理一些实在紧急的问题，鉴于中断、softirq之类的可以直接抢占进程运行，就会出现中断或者 softirq 负载重的时候导致进程的 starving，从而触发看门狗或者出现其它问题，因此，尽管中断、softirq 再忙，也要留出时间给进程执行，这是前提。 

对于 softirq 而言，当 softirq 的负载太重时，就需要将 softirq 放到进程环境下执行，也就是唤醒 softirqd，和其它进程放到一起调度，进程因此得到执行时间，唤醒 softirqd 的条件是：当前 softirq 执行完之后发现又有 softirq 处于 pending 状态，再执行三个判断：

* softirq 执行时间达到一定的时限
* need_resched 标志被置位
* softirq 连续执行的次数达到上限

这三个判断中任意一个不满足，就会唤醒 softirqd，如果都满足还是重新在中断环境下执行 softirq。  

我们主要来看第一个，softirq 执行时间实际上是通过 jiffies 判断的，也就是在 softirq 执行之初设置一个超时的 jiffies，jiffies 是在 tick 中断中更新的，如果 softirq 执行时间过长，当前 jiffies 就可能超过超时时间，从而唤醒 softirqd。 

但是，当 CPU 执行 IDLE 进程时，可以通过内核配置将 tick 中断停下来，尤其是 arm 平台下，这是出于省电的考虑，反正没有进程执行了，定时器是可以停一停的，当 CPU 的 runqueue 被 push 进来一个进程时，再恢复 jiffies，当然，中间停止的那段时间自然需要再加到 jiffies 上，这就可能导致在某一次 jiffies 的更新出现跳跃。 

因此，当 softirq 执行时，在判断 softirq 执行的时长是通过 jiffies，这就有可能导致 jiffies 是由于空闲的 CPU 上突然 push 进了一个进程而增加了好几个刻度，内核就会以为是 softirq 已经执行很长时间了，从而错误地唤醒 softirqd。

因此，在 irq_enter 中才会判断当前 CPU 是否正在执行 IDLE 进程，这种情况下就需要通过 tick broadcast 来更新一下 jiffies(内核 tick 相关的知识点可以参考这一系列文章)。

注2：再回到 __handle_domain_irq 中，CPU 通过读取 GIC 的寄存器获取当前中断的 hwirq，但是系统中对中断的处理都是基于内核唯一的逻辑 irq，因此需要通过 irq_find_mapping 找到当前 hwirq 对应的逻辑 irq，以便后续的处理。

注3：调用 generic_handle_irq(irq) 执行通用的 irq 处理，将会调用注册到内核中的中断处理函数，见下文。 

注4：最后调用 irq_exit，可以看出这是和 irq_exit 相对的，不难想到的是会将 preemt count 的中断标志位减一。 

紧接着将会触发软中断的执行：

```c++
if (!in_interrupt() && local_softirq_pending())
		invoke_softirq();
```

关于软中断的实现可以参考这篇文章(TODO)

中断状态位复位，对于用户而言，此时已经不处于硬件中断环境下了，而是进入了软中断的执行环境中，其实，什么是中断环境这个概念是相对模糊的，严格来说，CPU 从向量跳转开始就已经进入了中断环境中，而从中断点返回才算是退出中断环境。 

但是实际的软件实现并不是这样，中断环境被分为真正的中断处理和 softirq 处理(中断下文)两个部分，对于真正的中断处理，实际上处理完用户注册的回调函数之后就退出了，然后进入到 softirq 处理的阶段，对于硬件中断处理，全程都是关中断的，因此也就不可能发生嵌套，而对于 softirq，在执行的时候实际上是开中断的，因此，另一个中断可以抢占当前的 softirq 执行。

中断导致模式跳转到 irq mode，实际上只在 irq mode 作了短暂的停留，就跳转到 svc 模式下执行 high level 的中断处理，同时，softirq 是纯软件实现的机制，因此，softirq 被抢占的行为其实和普通的内核进程状态下被抢占的行为差不多，因此，当 softirq 被中断抢占时，该中断并不会再次调用 softirq，而是回到 softirq 被抢占的点，继续执行软中断，在 softirq 执行完成之后，才会完完全全地退出中断，回到中断之前的点。严格来说，这个时候才算是真正地退出了中断。

因此，中断的执行实际上也是一个执行流，它可以被抢占执行，但是这个执行流和进程具有完全不同的属性，因此它不能和进程一样参与调度，也就是中断环境不能睡眠。 





### 通用中断处理函数 generic_handle_irq

再回过头来看看上面留下的问题：__handle_domain_irq 中调用的 generic_handle_irq，这是中断的核心处理函数。 

顾名思义，这是一个通用的中断处理接口，传入的参数是该中断对应内核中的逻辑 irq。

在这个函数中，将通过 irq_to_desc 获取 irq 对应的 irq desc 结构，这个结构中保存了大部分中断相关的数据，包括用户注册的中断回调函数。 

接着调用 generic_handle_irq_desc 执行中断，实际上很简单，就是调用了 desc->handle_irq() 回调函数。 

这是个 high level 的回调函数，在中断系统初始化的时候被设置的，对应的函数为(参考[中断的注册](https://zhuanlan.zhihu.com/p/363135469))：

* handle_fasteoi_irq：针对 SPI 中断的 high level 中断处理函数
* handle_percpu_devid_irq：针对 PPI 中断的 high level 中断处理函数

* gic_handle_cascade_irq：针对级联的 GIC 中断的 high level 中断处理函数

接下来主要对 SPI 中断和级联中断的处理进行分析，而 PPI 中断的处理相对简单，和 SPI 中断处理类似，就不再啰嗦了。 

### 中断处理回调函数 handle_fasteoi_irq

通常听到的中断下半部一般是 softirq、tasklet 和 workqueue，其中 tasklet 实现是基于 softirq，而 workqueue 就是纯粹的内核线程环境。 

实际上，还有一种较新的中断处理措施就是中断线程化，也就是用户注册的中断 handle 在线程中执行而不是在中断上下文中执行，这种机制的产生是为了提高内核的实时性。  

怎么理解呢？首先，中断中需要执行任务的紧急度自然是不用再强调，这里的中断线程化并不是放到普通线程中，而是放到实时线程中，采用 FIFO 的调度方式，优先级为 49。

对于系统中非常紧急的任务，通常是使用实时进程来执行，但是如果这时候的中断负载也比较重的话，实时进程就很可能因为中断过多而导致执行不及时，造成严重的后果，采用中断线程化的机制，就可以将该实时进程的优先级提到比中断线程高，优于中断的执行，以这种方式提高实时进程的实时性，这种方式只是使开发者多了一种选择，至于这种中断线程化的方式怎么用最好还是需要在实际的应用场景中测试。 

但是，这并不等于实时进程运行时中断不会触发，中断的优先级依旧高于内核线程，这是毋庸置疑的，在产生中断时依旧会强制跳转到中断向量执行，只是用户传递的中断处理函数不再放在中断上下文中执行，而是线程唤醒，这两点要分清楚。 

中断线程化增加了中断的灵活性，同时，它也会带来一个问题：假设现在有个电平触发的中断源，低电平使能，对于经典的中断处理方式，执行流程为：

* 中断向量跳转
* high level 中断处理，向 GIC ack 中断
* 用户注册的回调函数，在这里会将中断源的电平复位，并执行其它预定义的动作
* 退出中断

但是对于中断线程化而言，从第三步开始就出现了问题，因为用户注册的回调函数被 push 到实时线程中执行，有一个唤醒线程的动作，但是中断执行流并不会因此停下等待，而是继续执行直到退出中断，因此很可能中断已经退出，而用户注册的回调 handle 还没执行，这就导致中断源的中断线还没有复位，从而再次触发中断。

因此，如果要对一个电平触发的中断执行线程化，需要增加一个 IRQS_ONESHOT 参数，这个参数会保证只有在中断处理函数真正处理完只有，才会再次开启该中断。内核中使用 mask_irq 和 unmask_irq 接口来禁用、使用特定中断。  

前面铺垫得有点长，这是因为中断的处理代码中包含了很多中断线程化的代码，发现没有办法抛开这个概念讲中断，干脆也就一起讲了。 

好了，再回过头来看中断的 high level 接口： handle_fasteoi_irq

```c++
void handle_fasteoi_irq(struct irq_desc *desc)
{
    ...
    if (desc->istate & IRQS_ONESHOT)
        mask_irq(desc);
    handle_irq_event(desc);
    ...
}

irqreturn_t handle_irq_event(struct irq_desc *desc)
{
    ...
	ret = handle_irq_event_percpu(desc);
}
```

按照惯例，其它的细枝末节、异常处理以及不重要的标志位设置就暂时不管了，关注主要部分. 

中断处理的调用路径为：handle_fasteoi_irq ->handle_irq_event->__handle_irq_event_percpu。而 \_\_handle_irq_event_percpu 就是我们想找的：

```c++
irqreturn_t __handle_irq_event_percpu(struct irq_desc *desc, unsigned int *flags)
{
	irqreturn_t retval = IRQ_NONE;
	unsigned int irq = desc->irq_data.irq;
	struct irqaction *action;

	for_each_action_of_desc(desc, action) {            
		irqreturn_t res;

		res = action->handler(irq, action->dev_id);   .........................1

		switch (res) {
		case IRQ_WAKE_THREAD:
			__irq_wake_thread(desc, action);          .........................2
		case IRQ_HANDLED:
			*flags |= action->flags;
			break;
		}
	}
    ...
}
```

 注1：中断处理要考虑两种情况，一种是常规的中断，另一种是共享中断，通常由于硬件上的限制，某些外设中断共享一根中断线，因此，软件上也是共享同一个 high level 的处理函数，high level 的中断处理再调用用户注册的对应的中断处理函数，所有注册的中断处理函数挂在 desc->action 上，irq desc 是 per irq 的，desc->action 中包含 next 指针，也就是支持链式结构，共享同一个中断线的不同中断源使用 dev_id 进行中断的识别。  

比较出乎意料的是，这里的中断处理使用了 for_each_action_of_desc，也就是如果是共享中断，会轮询整个 desc->action 列表，执行所有注册到系统中的回调函数，比如外设 A 和 B 共享一个中断，内核中也同时申请了这两种中断，回调函数分别为 Ahandle、Bhandle，当 A 触发中断时，实际的处理方式是 Ahandle 和 Bhandle 都会被执行，看起来有点奇怪。 

不过仔细一思考，好像确实也只能这样，当中断发生时，CPU 通过读取 GIC 寄存器可以知道当前发生的中断是哪个中断线，但是如果没有额外的辅助信息，CPU 并不知道是哪个中断源被触发，这时候就只有两种方案：一是提供额外的辅助信息，通过 GIC 也好、特定的内存区域也好，另一种就是当前的处理方式，轮询处理。 

实际上，硬件上一个中断线上共享的中断通常不会很多，而且也不大可能同时都注册到系统中，同时，如果开发者在共享中断的回调函数最开始通过 dev_id 来判断中断源，如果当前执行的中断处理程序和中断源不匹配，立马返回，实际上这种轮询的成本是非常小的，至少比通过设置其它额外的辅助信息的实现成本要低，只是看起来不那么"优雅"。 

注2：执行完中断处理程序之后有一个返回值，这个返回值通常是两种：IRQ_WAKE_THREAD 和 IRQ_HANDLED，IRQ_HANDLED 表示中断的正常处理，而 IRQ_WAKE_THREAD 表示该中断使用了中断线程化，需要唤醒中断线程执行该中断回调函数。对应的唤醒接口为 __irq_wake_thread。  



### 级联 GIC 的处理-gic_handle_cascade_irq

对于级联的 GIC，在 [arm GIC介绍](https://zhuanlan.zhihu.com/p/363129733) 就已经提到了，其实并不难猜到它的执行流程，在 root GIC 的中断处理中，通过 hwirq 获取对应的逻辑 irq，然后获取对应的 irq desc，再执行 irq desc 中的high level 中断处理函数，再执行注册的 irq handle。 

级联的 GIC 的中断线连在 root GIC 的某个中断引脚上，CPU 最先获得 root GIC 对应的 hwirq，同时 irq_find_mapping 获取对应的逻辑 irq，然后获取对应的 irq desc，前面是一样的，不一样的是 irq desc 的 high level 中断处理函数中是递归地处理 secondary GIC，也就是会读取 secondary GIC 中触发中断的 hwirq，再找到对应的逻辑 irq 和 irq desc，此时的 irq desc 中调用对应该中断的真正处理函数，其实就是深入一层，到 secondary GIC 中处理，对于多级串联的 GIC 一样的道理。  



上文中提到的都是中断的处理，从概念上来讲，PPI 和 SPI 很不一样，PPI 是 percpu 的，而 SPI 是共享的，PPI 和 SPI 都只会分发到一个特定的 CPU 来处理，因此对于 CPU 来说，处理起来都是一样的。

## SGI 中断的处理

SGI 基本上都是用于 CPU 核间的通信(又被称为 IPI 中断)，默认情况下，arm 平台中 SGI 的实现为：

```c++
enum ipi_msg_type {
	IPI_WAKEUP,
	IPI_TIMER,
	IPI_RESCHEDULE,
	IPI_CALL_FUNC,
	IPI_CPU_STOP,
	IPI_IRQ_WORK,  
	IPI_COMPLETION,
	IPI_CPU_BACKTRACE,
};
```

这个枚举列表就是 arm 平台中使用的 SGI 中断，第一项对应 0 号 SGI 中断，以此类推,因此,内核中使用了 0~7 号 IPI 中断,而 8~15 号并没有使用。

和 SPI 和 PPI 不一样的是, SGI 中断没有诸如电平触发还是边沿触发这样硬件上的考虑，也不需要考虑级联的 CPU，因为 secondary GIC 通常不使用 SGI，也没有必要,因此 SGI 的处理相对简单。   

从汇编层面到 C 函数调用的 gic_handle_irq 中,CPU 读取到 hwirq 小于 16 时,就直接进入了 IPI 中断的处理,对应的接口是 handle_IPI(irqnr, regs),这个函数接口非常直接明了,就是通过传入的 hwirq 分别对这几类 IPI 中断进行相应的处理:

```
void handle_IPI(int ipinr, struct pt_regs *regs)
{
	unsigned int cpu = smp_processor_id();
	struct pt_regs *old_regs = set_irq_regs(regs);
	...
	switch (ipinr) {
	case IPI_WAKEUP:
		break;

#ifdef CONFIG_GENERIC_CLOCKEVENTS_BROADCAST
	case IPI_TIMER:
		irq_enter();
		tick_receive_broadcast();
		irq_exit();
		break;
#endif

	case IPI_RESCHEDULE:
		scheduler_ipi();
		break;

	case IPI_CALL_FUNC:
		irq_enter();
		generic_smp_call_function_interrupt();
		irq_exit();
		break;

	case IPI_CPU_STOP:
		irq_enter();
		ipi_cpu_stop(cpu);
		irq_exit();
		break;

#ifdef CONFIG_IRQ_WORK
	case IPI_IRQ_WORK:
		irq_enter();
		irq_work_run();
		irq_exit();
		break;
#endif
	case IPI_COMPLETION:
		irq_enter();
		ipi_complete(cpu);
		irq_exit();
		break;

	case IPI_CPU_BACKTRACE:
		...
	default:
		break;
	}
	set_irq_regs(old_regs);
}
```

* 编号为 0 的 SGI 中断为 WAKE_UP 中断,很明显,如果 CPU 响应了这个中断,那么 CPU 也就被唤醒了,因此这个中断啥都不用干.
* 编号为 1 的 SGI 中断为 IPI_TIMER,这个中断是否实现取决于内核配置项 CONFIG_GENERIC_CLOCKEVENTS_BROADCAST,这是内核时间子系统相关的概念,和硬件架构强相关. 
  以 cortex A7 为例,硬件上每个 CPU 都配备了一个独立的 per core generic timer,这个 timer 并不是后续的 CPU 厂商添加的,而是集成在 arm 处理器中的,在内核中,这个 timer 专为 CPU 核心的调度提供稳定的 tick 中断.
  在某些情况下因为某些原因需要让 CPU 进入省电模式时,这个 tick timer 可能会被停止,以达到更高层次的省电效果,但是停掉 CPU 本地的 timer 同时也意味着系统中依赖于时间的处理工作无法完成,比如定时的唤醒动作,这时候就会用到系统提供的一个全局 timer,这个全局 timer 是 always power-on 的,充当临时的 timer 以完成那些必要的时间相关的工作,而这个全局的 timer 提供时间的方式就是通过这种 IPI 中断发送给 CPU,从 BROADCAST 后缀可以看出它并不针对某一个 CPU,而是可以实现广播发送到所有 CPU. 
* 编号为 2 的 SGI 中断为 IPI_RESCHEDULE,这个中断的作用在于向某个 CPU 发送消息,让目标 CPU 的执行相关的调度程序,个人感觉 RESCHEDULE 这个名称取得并不是很贴切,这很容易让人理解为这个中断是为了让调度器重新执行调度,实际上不是. 
  当 CPU 决定唤醒某个睡眠中的进程时,会为待唤醒的进程选择一个合适的 CPU,将其 push 到对应 CPU 的 rq 上,如果执行唤醒 CPU 与目标 CPU 共享缓存,也就是离得比较近,可以直接对目标 CPU 的 rq 进行加锁操作,但是如果这两个 CPU 离得比较远,从缓存的角度来看,会对执行唤醒的 CPU 上带来大量的无效缓存,同时,直接操作其它 CPU 的 rq 也会带来竞争.
  两相权衡之下,就采用另一种远程唤醒的方式,就是将该进程挂在目标 CPU 的 rq->wake_list 上,然后发送一个 IPI_RESCHEDULE 中断告诉目标 CPU:你现在最适合运行这个进程,你自己去把它唤醒吧.这就是 IPI_RESCHEDULE 的应用. 
* 编号为 3 的 SGI 中断为 IPI_CALL_FUNC,这个中断用于通知目标 CPU 执行某些预定义的回调函数,这些回调函数通过 smp_call_function_many 添加到 percpu  的 call_single_queue 队列上,当 CPU 接收到该中断时,就将队列上的回调函数一一取出来执行.
* 编号为 4 的 SGI 中断为 IPI_CPU_STOP,这个中断用于停止一个 CPU,调用 ipi_cpu_stop 函数,通过这个函数的源码可以看出,先将该 CPU 的 online 状态设置为 false,然后禁止 CPU 的 fiq 和 irq,最后执行一个 while(1) 的死循环,死循环中执行 cpu_relax(). 
* 编号为 5 的 SGI 中断为 IPI_IRQ_WORK,这个中断的支持需要内核配置项 CONFIG_IRQ_WORK 的使能,这个中断是比较罕见的,它的使用条件为:当某些函数需要在中断上下文环境下执行时,将其添加到 percpu 的 raised_list 和 lazy_list 链表上,该中断响应程序中将会逐一将其中的回调函数并一一执行,至于为什么有些函数会一定要放在中断上下文环境下执行,我也没想明白,不过可以参考[相关的官方文章](https://lwn.net/Articles/411605/)(我不是很理解里面关系 NMI 的解释).
* 编号为 6 的 SGI 中断为 IPI_COMPLETION,completion 在内核中是一种比较常见的同步方式,通常是针对进程,而这里的 completion 则是针对某些特定于 CPU 的工作,这个中断通常用于通知目标 CPU 某项工作的完成.
* 编号为 7 的 SGI 中断为  IPI_CPU_BACKTRACE,该中断打印相关的 trace 信息,暂不讨论. 



### SGI 中断的触发

IPI 中断的响应已经了解了,相对应的, CPU 是如何发送一个 IPI 中断的呢? 

在 GIC 的驱动初始化代码中,有这么一段关于 SMP 的初始化:

```c++
static int __init __gic_init_bases(struct gic_chip_data *gic,
				   int irq_start,
				   struct fwnode_handle *handle)
{
    ...
    #ifdef CONFIG_SMP
		set_smp_cross_call(gic_raise_softirq);
	#endif
    ...
}

void __init set_smp_cross_call(void (*fn)(const struct cpumask *, unsigned int))
{
	__smp_cross_call = fn;
}
```

这个 set_smp_cross_call 函数就是将 gic_raise_softirq 设置为发送 SGI 中断的函数,接受两个参数,一个是 cpumask,一个是 SGI 的 irqnum. 

比如向目标 CPU 发送一个 WAKE_UP IPI 中断时,调用路径为 arch_send_wakeup_ipi_mask->smp_cross_call->__smp_cross_call,也就调用到 gic_raise_softirq 回调函数,不得不说 gic_raise_softirq 这个函数名称乍一看还以为是和 softirq 相关的,实际上它 SGI 相关.关于这个函数的分析在 [arm gic 源码分析](https://zhuanlan.zhihu.com/p/363134084)中有介绍,这里就不再赘述了. 



### 参考

[蜗窝科技：中断处理流程](http://www.wowotech.net/irq_subsystem/irq_handler.html)

[中断相关的官方文档](https://www.kernel.org/doc/html/v4.12/core-api/genericirq.html)

4.14 内核源码



[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。



