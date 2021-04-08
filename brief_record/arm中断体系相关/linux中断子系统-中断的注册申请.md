# linux中断子系统 - 中断的注册

在编写内核驱动时,有时候会需要使用到外设的中断功能,需要向内核注册中断处理程序,一方面是使能该中断,另一方面是注册中断发生时的回调函数,一般来说,注册中断使用得最多的接口是 request_irq,还有一种并不太常用的中断线程化处理方式,也就是将中断处理函数放到线程中执行而不是中断上下文中,以提高系统中某些实时进程的实时性. 



## 申请中断的接口

向内核申请中断,首先需要知道该外设对应的中断号,这个中断号通常由设备树提供,驱动 probe 程序中通过解析设备树的 interrupts 属性获取中断号,然后调用 request_irq:

```c++
static inline int __must_check
request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,const char *name, void *dev)
```

handle 参数是用户传入的中断回调函数

flags 参数用于设置中断的相关属性,相应的参数有:

* IRQF_SHARED:表明该中断是共享的,也就是一个中断线对应多个外设中断源,在中断处理时需要执行额外的识别工作.
* IRQF_TIMER:标记该中断属于 timer 类型的中断,方便内核进行特殊处理
* IRQF_PERCPU:percpu 类型的中断
* IRQF_NOBALANCING:表示当前中断不受 irqbalance 的影响,实际上是通过限制对该 irq 设置 affinity 来实现的,irqbalance 并不是内核中的策略,而是一个用户空间的 demon,通常应用在 x86 架构中.该 demon 根据 CPU 的负载以及所处的模式(performance 或者 power-save)来动态地将系统中的中断绑定到不同的 CPU,其实现方式也是通过读写应用空间的 /proc 下的接口,比如通过 /proc/interrupts 了解系统中的中断信息,通过 /proc/irq/N/smp_affinity 设置中断的亲和性. 
* IRQF_IRQPOLL:中断用于轮询,出于性能考虑,只有在共享中断中注册的第一个中断才设置.(这句话只是翻译了内核中的注释,但是我并不知道这个标志位具体是什么意思,走过路过的哪位了解的大神可以指导指导).
* IRQF_ONESHOT:只有当中断真正处理完成之后,才会再次开启中断,主要是应对中断线程化中的一些问题,后面会再提到.
* IRQF_NO_SUSPEND:表示在系统 suspend 的时候不要 disable 该中断,也就是说这个中断可以用来在系统休眠的时候用作系统的唤醒,但是也并不一定保证该中断能唤醒睡眠中的系统,还需要使能 irq 唤醒功能(enable_irq_wake),具体参见[内核文档](https://github.com/torvalds/linux/blob/master/Documentation/power/suspend-and-interrupts.rst)
* IRQF_FORCE_RESUME:在系统 resume 时 enable 该中断
* IRQF_NO_THREAD:中断不能被线程化
* IRQF_EARLY_RESUME:在系统 resume 的时候,该中断在早起被 resume 而不是在设备唤醒阶段

name 参数在中断处理的过程并不需要,只是用于在显示或者调试的时候方便识别中断

dev:也就是 dev_id,用于唯一标示一个中断,对于独立的中断通常设置为 NULL,主要针对共享中断,在共享中断的处理函数中通过 dev_id 判定具体的中断源,在中断 free 的时候也通过该 id 才能识别到具体要释放的中断源.

request_irq 的源码如下:

```c++
static inline int __must_check
request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
	    const char *name, void *dev)
{
	return request_threaded_irq(irq, handler, NULL, flags, name, dev);
}
int request_threaded_irq(unsigned int irq, irq_handler_t handler,
			 irq_handler_t thread_fn, unsigned long irqflags,
			 const char *devname, void *dev_id);
```

尽管注册中断和注册中断线程是完全不同的两种处理方式,但是在内核中将这两种处理方式放在一个函数中处理,对于 request_irq 注册中断而言,只需要提供中断的 handle,而不需要提供 thread_fn.

如果注册中断线程,调用 request_threaded_irq 接口,需要提供 thread_fn,而中断 handle 是可选的,如果不提供,则使用系统默认的 handle,如果提供,该 handle 会在中断上下文中执行,如果该 handle 被正确编写,再执行 thread_fn. 

 

## request_threaded_irq

request_threaded_irq 是注册中断的主要函数,该函数主要实现如下:

* 一些必要的检查工作,比如如果设置共享中断标志则必须提供 dev_id.又比如判断该中断是否能注册,如果该中断不是共享中断且已经被注册,就不能再次注册,返回失败. 

* 通过传入的 irq num,调用 irq_to_desc 函数获取到对应的 irq_desc,这个 irq_desc table 是在 GIC 驱动初始化阶段生成的,每一个有效的 irq num 都对应一个 irq desc,irq desc 这个结构记录(或连接)了一个 irq 的所有相关信息.

* 当 handle 和 thread_fn 都没有提供时,返回错误.
  如果只提供 thread_fn, handle 被赋值为系统默认的 irq_default_primary_handler,表示中断线程化的情况.
  如果只提供 handle,就是标准的注册中断 handle,handle 在中断上下文中被执行
  如果两者都被提供,中断线程会被创建,但是是否会将中断放到线程中执行取决于 handle 的实现. 

* 构建一个 irqaction 结构,irq desc 负责对整个中断的描述,而 irqaction 则针对用户注册中断部分的描述,该数据结构中包含了在注册中断时传入的信息:

  ```c++
  struct irqaction {
  	irq_handler_t		handler;
  	void			*dev_id;
  	struct irqaction	*next;
  	irq_handler_t		thread_fn;
  	struct task_struct	*thread;
  	unsigned int		irq;
  	unsigned int		flags;
  	unsigned long		thread_flags;
  	unsigned long		thread_mask;
  	const char		*name;
  	struct proc_dir_entry	*dir;
      ...
  }
  ```

  该结构中大部分成员都是传入的参数,有几个参数需要注意:

  * next: 通过 next 指针可以看出,对于内核中某一个 irq 中断,是可以注册多个 irqaction 的,也就是共享中断的情况,这些 irqaction 通过 next 指针连接.
  * dir : 对于被注册的中断,会在 /proc 目录下导出相应的接口,对应的目录为: /proc/irq/${IRQ_NUM},这个数据结构主要用于导出数据

* 执行 __setup_irq 函数,可以看出这个函数就是初始化一个 irq 的核心函数,传入的参数为 irq num,对应的 desc 以及新生成的 action 结构. 

request_threaded_irq 的源码就不贴了,有兴趣的可以自己去翻翻. 



## __setup_irq 初始化中断

__setup_irq 是初始化中断的核心函数,是一个很长的函数,按照惯例,针对这个函数,主讲逻辑,只贴出主要的代码,该函数接受三个参数

* irq num : 中断源对应系统中的逻辑 irq,这是这个 irq 的唯一标识
* irq desc: 该 irq 对应的 desc 结构
* new : 通过用户注册中断时传入的 handle,thread_fn,flag, dev_id 等参数生成的一个 irqaction.

不难猜到,初始化中断主要的工作就是:

* 将新生成的 irqaction 挂到对应的 irq_desc 上,毕竟在中断的处理流程中提到,中断处理的过程中就是间接调用了 irq desc 中的 action 回调接口

* 使能中断,这里的使能中断只针对 GIC 的配置,也就是使能中断源-> CPU interface -> CPU核 的传递过程,实际上中断是否可用还取决于另外两个条件:

  *  相应的硬件控制器是否配置中断使能,也就是发生相应的事件时是否会产生中断信号并发送到 GIC
  * CPU 是否使能中断,如果 CPU 中断被屏蔽,中断信号也不会被中断处理

  从系统的角度来看,只有这三个条件都达成,才能确保一个中断源的正常处理,硬件控制器的中断配置由对应的硬件驱动完成,而 CPU 中断由系统软件代码控制.
  这里仅仅是做 GIC 相关配置. 

不过呢,实际上初始化中断的处理要相对复杂一些,这也是 linux 的特点:对任何一个组件的处理,通常都要综合考虑多核并发,性能优化,硬件的兼容,通常具体的实现都是比较繁琐的.  

因此 \_\_setup_irq 是一个非常长的函数实现,直接贴上代码估计也没人看,按照惯例还是一部分一部分的讲解,贴上关键代码作为辅助讲解.首先,作为一个概括性的总结,\_\_setup_irq 执行的流程如下:

* 日常的异常处理和检查工作,这部分不准备深入讲解
* 解决中断 nested 的问题,不要误会,这里的 nested 并不是中断嵌套,而是针对某一类特殊的中断控制器做兼容
* 中断线程化的处理,中断线程化是内核中针对进程实时性引入的机制
* 共享中断的处理与非共享中断的区别处理



### nested 中断控制器的处理

其实用 nest 中断这个表述很容易引起误会,以为是中断嵌套,实际上中断嵌套在 2.6 内核之后就不再被主线代码支持了,这里的嵌套指的是中断控制器的嵌套,而是还是非常特殊的那一类的中断控制器.  

在 gic 驱动(TODO) 章节就说到, arm 的 gic 是可以级联的,也可以说成是嵌套,这类 gic 的级联是比较好解决的,在 CPU 处理中断时根据 root gic 的中断 handle 进入到 secondary gic 中递归操作 root GIC 即可,因为 gic 都是 Memory map 类型的,CPU 对 gic 的读写速度没有限制,速度非常快.  

但是在某些系统上,中断控制器可能不是 Memory map 类型的,而是 i2c 或者 spi 一类的慢速接口, 当这一类中断控制器上的中断触发时, 可以想到, CPU 同样需要递归进入到该中断控制器中读取相关数据以确定是哪个中断源被触发了. 

这种慢速接口的"慢"并不是主要问题(当然也算比较重要的问题),主要问题是这一类总线的读写操作会引起睡眠,比如 i2c 在发送 start 信号之后会睡眠等待, 而中断处理中自然是不允许睡眠的,不难想到,这种情况就需要将这个中断控制器对应的中断进行线程化,在线程中处理这些中断.  

再深入考虑会发现,假设是一个 i2c 中断控制器, CPU 在发送 i2c 数据读取数据时,会触发 i2c 中断,那为何不直接在 i2c 的中断处理程序中处理这些中断问题呢?于是,这种类型中断的处理就变成了: 将 i2c 中断处理线程化,而 i2c 中断控制器连接的所有中断线都在 i2c 中断线程中处理.因此,当前中断就没有必要再使用一个内核线程单独处理了. 

nested 中断控制器相关代码为:

```c++
static int
__setup_irq(unsigned int irq, struct irq_desc *desc, struct irqaction *new)
{	
    ...
	nested = irq_settings_is_nested_thread(desc);
	if (nested) {
		if (!new->thread_fn) {
			ret = -EINVAL;
			goto out_mput;
		}
		new->handler = irq_nested_primary_handler;
	}
    ...
}
```

在中断子系统初始化的时候,如果存在这种非常特殊的中断控制器,就会在相关的 irq desc 中设置相应的标志,因此 irq_settings_is_nested_thread 函数其实是读取 desc 中的相关标志以确定当前中断是不是 nested 中断. 

如果是 nested 中断,需要提供 thread_fn,同时将 handler 设置为一个无效的 handle,该 handle 什么都不做,只是返回 IRQ_NONE,实际上这种情况下 thread_fn 也不会被调用到,为什么还需要提供 thread_fn?可能是为了限制对这个 irq 使用 request_irq 接口.  

 

### 中断线程化的处理

正如上文中提到的,中断线程化就是将用户提供的中断处理函数放到实时的内核线程中执行,中断依旧会触发,只是中断上下文中的处理部分变得非常快而且时间可控,这就为系统的实时性做出了贡献,当然,linux 中实时性并不仅仅是中断处理时间的问题,另一个更重要的问题是内核中无处不在的关中断,因此,中断线程化也没能解决 linux 内核实时性的问题,只是有相应的提高.  

中断线程化有一个内核配置选项是 CONFIG_IRQ_FORCED_THREADING,这个内核选项决定了系统中的中断是不是会被强制线程化,如果该选项配置为 Y,意味着注册的中断默认都会创建一个线程. 



在 __setup_irq 函数的处理中,对强制线程化的处理是这样的:

```c++
static int
__setup_irq(unsigned int irq, struct irq_desc *desc, struct irqaction *new)
{	
    ...
    nested = irq_settings_is_nested_thread(desc);
    if (nested) {...}
	else{
        if (irq_settings_can_thread(desc)) {
			ret = irq_setup_forced_threading(new);
			if (ret)
				goto out_mput;
		}
    }
    ...
}
```

如果强制中断线程化被设置,调用 irq_setup_forced_threading 函数:

```c++
static int irq_setup_forced_threading(struct irqaction *new)
{
	if (!force_irqthreads)
		return 0;
	if (new->flags & (IRQF_NO_THREAD | IRQF_PERCPU | IRQF_ONESHOT))   .............1
		return 0;

	new->flags |= IRQF_ONESHOT;

	if (new->handler != irq_default_primary_handler && new->thread_fn) {    .......2
		new->secondary = kzalloc(sizeof(struct irqaction), GFP_KERNEL);
		if (!new->secondary)
			return -ENOMEM;
		new->secondary->handler = irq_forced_secondary_handler;
		new->secondary->thread_fn = new->thread_fn;
		new->secondary->dev_id = new->dev_id;
		new->secondary->irq = new->irq;
		new->secondary->name = new->name;
	}
	set_bit(IRQTF_FORCED_THREAD, &new->thread_flags);
	new->thread_fn = new->handler;
	new->handler = irq_default_primary_handler;
	return 0;
}
```

注1:内核配置强制中断线程化只是一个默认的配置,如果用户决定不使用中断线程化,还是得尊重用户的决定,有三种情况是不需要强制线程化的:

IRQF_NO_THREAD 很好理解,就是直接指定不创建内核线程. 

IRQF_PERCPU 不是外部中断,而是绑定到每个 CPU 的,这种中断并不大适合线程化,假设一个 8 核的系统,每一个 percpu 的中断线程化就得创建 8 个内核线程,明显得不偿失.

IRQF_ONESHOT 这个标志表示对于电平触发的中断,需要在中断处理程序完全处理完之后才能开中断,指定这个标志就说明了该中断一定会被线程化,而用户指定的线程化与强制线程化并不是统一处理,因此这里并不会处理.

对于强制线程化的中断而言,需要设置 ONESHOT 标志.也就是原本带有 ONESHOT 标志的中断不强制线程化,而强制线程化的在线程化过程中需要加上 ONESHOT 标志. 对于实际的处理来说,ONESHOT 类型的中断处理就是在处理器 mask 中断,用户处理完成之后再 unmask 中断. 

注2:对于强制线程化的中断而言,如果用户注册时使用 request_threaded_irq 同时给出了 handler 和 thread_fn(request_irq 函数指定 thread_fn 为 NULL),这种情况比较特殊,说明注册者既希望在中断上下文中处理一部分,又希望在内核线程中处理一部分.这时候使用了 action 中的以新的 struct irqaction secondary 成员来记录一个新的线程,用来运行传入的 thread_fn. 

普通情况下,该函数对用户传入的 handler 和 thread_fn 进行了修改:用户传入的 handler 变成了线程的 thread_fn,而对应的 handler 赋值为系统默认的 irq_default_primary_handler,根据中断处理流程章节(TODO)的描述, action->handler 会被执行,该函数返回 IRQ_WAKE_THREAD 时就会唤醒线程,这时候用户传入的 handler 就放到内核中执行了. 

强制中断线程化这个配置选项感觉有点一刀切的意思了,驱动开发者在编写驱动程序的时候,通常会假设中断处理程序运行在中断上下文中,以这个前提来做一些并发或者优化的处理,把这些处理函数放到线程中处理倒也不是不行,只是感觉有些怪,暂时没见过使用强制线程化配置的系统.

在默认不支持强制线程化的系统中,用户要创建中断线程就通过 request_threaded_irq 函数注册中断,传入一个thread_fn 函数作为回调函数,但是这个函数并不是该线程的执行函数,和我们使用 kthread_create 传入的 thread_fn 不是一个概念,只是作为一个在线程中会被调用到的函数. 

接下来,\_\_setup_irq 中就会为中断创建线程,对应的源码为:

```c++
static int
__setup_irq(unsigned int irq, struct irq_desc *desc, struct irqaction *new)
{	
	if (new->thread_fn && !nested) {
		ret = setup_irq_thread(new, irq, false);
		...
		if (new->secondary) {
			ret = setup_irq_thread(new->secondary, irq, true);
			...
		}
	}
}
```

如果 secondary 存在,也会为其创建对应的线程,setup_irq_thread 就是基于 kthread_create 的封装,创建线程的同时设置当前内核线程为 FIFO 调度策略的实时线程,优先级默认为 49,linux 中实时进程(内核线程)的优先级决定了它可以随时抢占普通进程执行,毕竟中断处理函数自然要拥有高优先级.这样的话,那些更紧急的进程就可以将优先级设置得比中断线程更高,以获得更好的实时性. 

在用户空间通过 ps -ef | grep "irq/*"  就可以看到系统中目前注册的所有中断线程.  

在 __setup_irq 的核心初始化工作中，先处理中断 trigger level，在注册并申请中断时，可以通过 flag 标志位设置中断的触发类型，这个中断触发类型指的是中断源的触发类型，而不是 CPU interface 到 CPU 的触发类型。

如果没有设置中断触发类型，就使用默认的 GIC 驱动中初始化的电平触发方式，GICV2 中为低电平触发。 



### 共享中断与非共享中断的区别处理

共享中断和非共享中断的区别是很明显的,非共享中断占用一个单独的 irq,一个萝卜一个坑,而共享中断则是多个中断源共享一根 GIC 的中断线,处理上的区别在于:

* 在中断处理上,非共享中断执行用户传入的唯一的 action->handler 或者 action->thread_fn,而共享中断可以存在多个 action,而且在执行时会执行所有注册到当前 irq 上的 action->handler 和 action->fn,至于到底是其中哪个中断源触发了,由用户自己判断解决. 
* 如果是中断线程化,多个共享的中断源只会创建一个线程,多个共享中断的第一个注册的驱动将会创建这个线程,而后续的其它共享中断只是复用这个内核线程.
* 对于非共享中断,重复的注册会报错,而共享中断可以对同一个 irq num 注册不同的中断源,新注册的 action 会通过 action->next 指针被添加到 action 链表上. 

对于非共享中断,或者共享中断中第一次注册,就需要进行一些必要的初始化工作,

* 调用 irq_request_resources 获取中断资源,实际上调用了对应的 chip->irq_request_resources 函数,这是架构相关的处理函数,和中断控制器相关,从名称来看就是申请中断资源或者标示独占某些中断资源,以解决或避免资源竞争,arm gic 并没有实现这个函数.  
* 初始化 desc->wait_for_threads 等待队列, 用于中断的同步,比如内核中执行 irq 的 disable 或者 free 时,需要等待最后一次中断的完成,等待的进程就会睡眠在该等待队列上. 
* 设置触发模式,用户在注册时可以通过传入 IRQF_TRIGGER_LOW/IRQF_TRIGGER_HIGH 等标志位设置中断源的触发方式,如果用户没有设置,就使用默认的触发方式,这个默认方式由 gic 驱动中初始化为低电平触发. 
* 中断的使能:如果中断不是 nested 类型,就需要对中断进行使能,对于 arm 而言,中断使能就是设置 GIC 的相关寄存器,按照惯例自然不是直接操作 gic 寄存器,而是通过调用 gic chip 的回调函数进行设置. 
* 设置中断的 CPU 亲和性,一般来说,中断会被默认设置分发到一个 CPU 上,而不会分发多个 CPU,毕竟如果分发到多个 CPU,一个中断源触发多个 CPU 进入到中断处理模式,但是最后只有一个 CPU 真正执行处理,这是一种浪费.
  同时,用户还可以设置中断是否可迁移,中断的 target CPU 可以通过用户空间的 /proc 目录下的文件进行修改,用户空间也存在 irq balancing 的 demon 进程,该进程可以根据 CPU 负载动态调整中断的 CPU 亲和性,基于某些实际应用的考虑,用户可以设置禁止中断在 CPU 之间的迁移. 

对于不是第一次注册的非共享中断,并不需要做什么初始化工作,只需要执行一些检查工作以确定新注册的共享中断是否与之前的中断相冲突,如果是则报错,然后将新的 action 添加到 action 链表上. 



最后,初始化该中断相关的统计参数,比如 irq_count/irqs_unhandled,再将中断导出到用户空间,整个中断的初始化工作也就完成了. 



### 中断线程的处理细节

当用户指定线程化中断或者强制中断线程化时,会调用 setup_irq_thread->kthread_create 创建一个中断内核线程,下面来看看这个中断内核线程的处理细节.  

首先,创建中断线程对应的源码为:

```c++
static int
setup_irq_thread(struct irqaction *new, unsigned int irq, bool secondary)
{
    struct task_struct *t;
    ...
    t = kthread_create(irq_thread, new, "irq/%d-%s", irq,
				   new->name);
    ...
}

```

对应的线程函数为 irq_thread:

```
static int irq_thread(void *data)
{
	struct callback_head on_exit_work;
	struct irqaction *action = data;
	struct irq_desc *desc = irq_to_desc(action->irq);
	irqreturn_t (*handler_fn)(struct irq_desc *desc,
			struct irqaction *action);

	if (force_irqthreads && test_bit(IRQTF_FORCED_THREAD,
					&action->thread_flags))
		handler_fn = irq_forced_thread_fn;
	else
		handler_fn = irq_thread_fn;

	init_task_work(&on_exit_work, irq_thread_dtor);
	task_work_add(current, &on_exit_work, false);

	irq_thread_check_affinity(desc, action);

	while (!irq_wait_for_interrupt(action)) {        ........................1
		irqreturn_t action_ret;

		irq_thread_check_affinity(desc, action);

		action_ret = handler_fn(desc, action);        .......................2
		if (action_ret == IRQ_HANDLED)
			atomic_inc(&desc->threads_handled);
		if (action_ret == IRQ_WAKE_THREAD)
			irq_wake_secondary(desc, action);         .......................3

		wake_threads_waitq(desc);
	}
	...
}
```

注1:irq_thread 的 data 参数为 action ,在申请时 action 可能有两种,一个是正常的由用户传递参数生成的 action,另一个是强制线程化生成的 action.  

根据 action 类型的不同,handle_fn 被赋值为不同的回调函数 ,这部分属于内核线程的初始化部分.

接下来就是一个 while 循环,判断 irq_wait_for_interrupt 的返回值,这个函数中也是循环地判断 action->thread_flags 中的 IRQTF_RUNTHREAD 标志位是否被置位,如果该标志没有被置位,线程将陷入 TASK_INTERRUPTIBLE 类型的睡眠.  

那么,谁来将这个标志位置位并唤醒当前线程呢?

还是要把目光放回到中断的处理流程这篇文章(TODO)中,回顾一下 lowlevel 的中断处理函数:

```c++
irqreturn_t __handle_irq_event_percpu(struct irq_desc *desc, unsigned int *flags)
{

	for_each_action_of_desc(desc, action) {
		irqreturn_t res;
		res = action->handler(irq, action->dev_id);
		switch (res) {
			case IRQ_WAKE_THREAD:
				__irq_wake_thread(desc, action);
        ...
		}
        ...
	}
}
```

也就是说,lowlevel 中断处理部分,不论是普通中断处理还是线程化的中断处理,action->handler 还是会被执行,只不过对于线程化中断,中断处理的返回值为 IRQ_WAKE_THREAD,这时候将调用 __irq_wake_thread 设置 IRQTF_RUNTHREAD 标志位,并执行 wake_up_process(action->thread) 唤醒中断线程. 

注2: 中断线程被唤醒之后,将会调用初始化阶段赋值的 handler_fn,如果是强制线程化的中断,对应 irq_forced_thread_fn 否则对应 irq_thread_fn. 

这两个函数的处理其实差不大多,都是调用 action->thread_fn 函数,也就是用户传入的回调函数,不同的是,如果是强制中断线程化的处理,需要关中断下半部,下半部主要是 softirq(tasklet 基于 softirq),为啥呢?

这是因为用户调用 request_irq 接口注册一个中断,但是内核强制中断线程化将这个中断处理函数放到了线程中,这就有可能导致:在用户看来,我的中断处理函数就是在中断上下文中执行的,因此优先级肯定比下半部高,在实际编码过程中也是依据了这一准则,但实际上放到线程中执行之后,优先级就比中断下半部低了,这就会导致死锁或者其它的问题.  

注3:如果上述的 handler_fn 依旧返回 IRQ_WAKE_THREAD,那么就会唤醒 secondary action,也就是在支持强制中断线程化的系统中,用户在注册中断时既传入了 handler 又传入了 thread_fn,而且 handler 的返回值为 IRQ_WAKE_THREAD,这种情况下,用户就可以把一部分工作放到中断环境下运行,另一部分工作放到内核线程环境中执行. 

在所有工作处理完成之后,调用 wake_threads_waitq 唤醒所有等待当前中断执行完成的进程. 