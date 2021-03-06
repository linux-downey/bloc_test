# linux调度子系统5 - 周期性调度器
对于目前市面上的大多数操作系统，都存在时间片轮转的调度方式，而使用时间片的初衷在于，系统通过时间片可以很方便地度量一个进程应该运行多久和一个进程已经运行了多久，以此作为唯一的或者辅助的依据来执行任务的调度.   

通常来说，时间片是由 tick 定时器实现的，选取一个固定的时间间隔，产生定时中断，一次中断的产生代表一个时间片的逝去，系统统一在中断处理过程中来对运行进程的时间片进行递减，该进程的可执行时间慢慢地减少直到最后让出执行权.在这里， tick 定时器看起来扮演了一个 timeline(时间刻度) 的角色，所有的进程都参照这个 timeline 进行调度.tick 产生的时间间隔由 HZ 配置.  

但是，对于调度而言，如果系统仅仅是需要一个 timeline 的话，tick 定时器也不是必须的，硬件上并不缺能够作为 timeline 的 counter，而且时间精度远高于 tick 定时器中断，从上电开始累加，同样地可以作为参照，一个运行中的进程完全可以通过检查 timeline 的方式来确定自己是否需要让出 CPU，而不需要通过系统实现一个 tick 定时器的方式.当然，我们都知道这样不合理，一方面是实现的复杂性，另一方面而言，在调度行为上，不应该依赖于进程的自觉性，而是让系统统一管理.   

因此，一个 tick 定时器的作用在于:



* 提供一个进程视角的 timeline
* 系统通过该 tick 回调函数，对进程以及调度行为进行统一管理

早期版本的 linux 内核中，也存在时间片的调度策略，自从引入 CFS 调度器之后，时间片的概念就从内核中消失了，取而代之的是虚拟时间，调度器通过虚拟时间来确定哪个进程应该投入运行，而虚拟时间并不是以 tick 的时间刻度进行计算的，因此 CFS 调度器中，tick 定时器作为 timeline 的功能被弱化了.   

tick 中断最大的一个作用在于，周期性地更新系统中的进程时间信息，然后检查当前进程是否需要调度，检查调度这个行为直接影响到进程运行的时间粒度，检查调度的频率越高，进程的理想运行时间和实际运行时间越接近，假设调度器分配给一个进程的运行时间为 2ms，如果检查调度的时间间隔为 10ms(100HZ)，那么该进程很可能就会运行完整个 10ms(没有出现抢占的情况下)，而如果将这个检查间隔降低到 2ms，就比较合适.   

实际的情况往往不是单方面的，配置高频率的 tick 中断，带来功耗问题的同时，还是增加进程切换的频率，增加系统负担，因此 HZ 值通常配置为 100 或者 250，即 10ms 或 4ms 一个 tick 中断，也有比较少见的将 HZ 设置为 1000 的情况，这往往需要根据实际场景进行设置.  

周期性调度的核心源代码位于 kernel/sched/core.c 中的 scheduler_tick 中，这个函数在系统中会被周期性地调用，而这个调用的间隔时间则取决于系统中 HZ 的定义，HZ 值代表了该函数被调用的频率，当 HZ 被设置为 250 时，该函数就会以 4ms 的间隔周期调用，每执行一次周期性调度，系统中的全局变量 jiffies 加 1.  

scheduler_tick 为什么会周期性地被调用，它又是被谁调用的? 这是个重要的问题，可以参考[周期性调度器timer](https://zhuanlan.zhihu.com/p/363787529)，这里我们只需要以下几点:



* 在系统的初始化阶段，会初始化并注册系统的调度 timer，这个 timer 具有 counter 的功能，同时也可以通过配置作为定时器使用，就目前大部分平台而言，这个定时器的精度为 ns 级别.在多核架构中，这个 timer 通常是 percpu 的，也就是每个 CPU 核独立的.  
* 在多核架构中，各 CPU 核的调度行为是相互独立的，因此 scheduler_tick 函数中大多数的内容都只和当前 CPU 的当前 runqueue 相关. 
* 尽管周期性调度本身是以 HZ 为刻度，但是内核进程的时间计数并不是以对应的 jiffies 为单位，通常都是直接使用硬件时间戳，精确到 ns 级别，而不是精度非常低的 jiffies



## 周期性调度器做哪些事
周期性调度做哪些事?这个问题也并不难回答，主要是两个方面:



* 更新时间，包括就绪队列的 clock，以及进程的虚拟时间 vruntime.
* 检查是否需要重新调度，如果需要，设置调度标志.为什么不是直接调度呢?这是因为 scheduler_tick 是在定时器中断中执行的，不能直接执行调度函数，在中断返回的时候会检查调度标志，执行调度，因此这个延迟时间并不会太高.  




## 周期性调度的实现
周期性调度是内核核心调度部分的内容，它并不针对某个调度器类，而是在适当的时候调用调度器类的回调函数.接下来看看 scheduler_tick 的源码实现. 

```c++
void scheduler_tick(void)
{
	int cpu = smp_processor_id();
	struct rq *rq = cpu_rq(cpu);
	struct task_struct *curr = rq->curr;
	struct rq_flags rf;

	sched_clock_tick();

	rq_lock(rq， &rf);

	update_rq_clock(rq);
	curr->sched_class->task_tick(rq， curr， 0);

	rq_unlock(rq， &rf);
    ...
}
```

对于 scheduler_tick 的函数实现，除了主要的调度逻辑，还包含了和调度相关的一些其它事物处理，包括:



* CPU 负载更新
* 系统跟踪，调试相关代码处理
* 在多核架构下，需要检查 CPU 核之间的负载均衡，以确定是否需要进行负载均衡
* NO_HZ 的处理，这是内核中的一种省电配置方式，当 CPU 上没有或者仅有一个进程运行时，可以配置关闭 tick 定时从而达到节省功耗的目的.

鉴于本系列文章的目的在于 cfs 调度器执行逻辑的分析，因此省略了上述处理部分，在后续的源码分析中也都会才减掉一些不必要的一份，专注于核心逻辑的分析.  




### 更新时间
scheduler_tick 中和更新时间相关的接口主要是两个，sched_clock_tick() 和 update_rq_clock(rq)，前者用来更新和调度 timer 相关的计时统计，后者用来更新 percpu 的 runqueue 的时间.以下是源代码:

scheduler_tick->sched_clock_tick
```c++
void sched_clock_tick(void)
{
	struct sched_clock_data *scd;
	scd = this_scd();
	__scd_stamp(scd);
	sched_clock_local(scd);
}
```
其中， scd 是一个 struct sched_clock_data 的结构，从名字可以看出，这是和硬件 timer 本身相关的一个计时数据结构，该结构是 percpu 的，也就是每一个 cpu 核都拥有各自的实体，其中记录了 raw_tick 和 clock 值，raw_tick 是硬件原始值，也就是 timer 的时间戳，而 clock 是经过过滤处理的.  

更新 scd 的函数主要是 \_\_scd_stamp(scd) 和 sched_clock_local(scd)，这两个函数都调用了 sched_clock() 获取当前硬件 timer 的时间，\_\_scd_stamp 中将硬件 timer 的时间用来更新 scd->raw_tick，sched_clock_local 中对 scd->clock 进行更新.  

sched_clock 是获取时间的底层接口，获取时间的上层函数会在多处被调用，都是基于 sched_clock 的封装，这个接口直接负责调用系统timer 的 read 回调函数，读取该定时器对应的时间戳，该时间以 ns 为单位.而 read 回调函数正是 schedule timer 在注册时提供的回调函数，用来直接读取 timer 的时间戳.  

除了维护 scd 类型的时间之外，更重要的是维护 CPU runqueue 的时间，这是和调度直接相关的参数， runqueue 时间的更新由 update_rq_clock(rq) 执行:

scheduler_tick->update_rq_clock
```c++
void update_rq_clock(struct rq *rq)
{
	s64 delta;

    delta = sched_clock_cpu(cpu_of(rq)) - rq->clock;
    if (delta < 0)
        return;
    rq->clock += delta;
    update_rq_clock_task(rq， delta);
}
```
在 [CPU 就绪队列章节](https://zhuanlan.zhihu.com/p/363785182)就已经提到了，struct rq 中有两个成员用来记录当前 CPU runqueue 的时间，一个是 clock，一个是 clock_task，clock 是一个递增的时间，表示 CPU runqueue 从初始化到现在的总时间，而 clock_task 则记录的是纯粹的所有进程运行的总时间，一个 CPU 上并不只是进程在运行，同时还有中断以及部分中断下半部.这部分的统计取决于内核配置，当内核配置了 CONFIG_IRQ_TIME_ACCOUNTING 时，rq->prev_irq_time 用来统计中断时间，而 clock_task 就是纯粹的 task 时间，在没有配置的情况下，clock 和 clock_task 不会区分中断用时. 

sched_clock_cpu 同样是调用了底层函数 sched_clock 获取 timer 的时间戳，rq->clock 保存的是上次时间统计的值，两者之间的 delta 再添加再与原来的 clock 相加，对于 clock_task 的更新也是一样的.  

这里为什么不直接进行赋值，而是先计算 delta，再将 delta 加上原值呢? 鉴于 timer 的精度问题，或者当硬件处于一些特殊环境下时可能出现的一些抖动，总之，timer 不一定总是像我们想象中那样完美地运行，硬件的问题需要谨慎地通过软件来修正，否则系统中的 timeline 出现倒退，会出现一些意想不到的问题.  




### runqueue 锁
不论是在 scheduler_tick 中还是在其它地方，对 CPU runqueue 执行写操作时都需要加锁，runqueue 中提供了一个 spinlock 用于保护 rq 的读写，需要注意的是，在考虑并发的时候，尽管 rq 是 percpu 类型的数据，但是并不代表只有当前 CPU 才能访问它，一个 CPU 是可能在任何时候访问其它 CPU 的 runqueue 的，比如在进程迁移的时候.  


### task_tick 函数
在 scheduler_tick 中，最为核心的函数就是 curr->sched_class->task_tick() 了，这个函数会根据当前进程的属性进入对应的调度器类，调用该调度器类的 task_tick 回调函数，目前我们关注的是 cfs 调度器类，其对应的 task_tick 函数定义在 kernel/sched/fair.c 中.  

scheduler_tick->task_tick_fair
```c++
static void task_tick_fair(struct rq *rq， struct task_struct *curr， int queued)
{
	struct cfs_rq *cfs_rq;
	struct sched_entity *se = &curr->se;

	for_each_sched_entity(se) {
		cfs_rq = cfs_rq_of(se);
		entity_tick(cfs_rq， se， queued);
	}
}
```

从名称可以看出，for_each_sched_entity 是一个循环条件，表示遍历每个调度实体 se，这里的"每一个"其实和组调度相关，一个调度实体并不一定是 task，而可能是一个 task_group，而一个 task_group 又维护一个独立的 cfs_rq，因此，可能出现嵌套.  

当执行进程选择时，若一个调度组 se 被选中，暂且叫他为 A，它需要继续递归地遍历 cfs_rq 上的所有调度实体，直到找到一个对应具体 task 的调度实体 B，这种情况下，从 root cfs_rq 的视角上看来， A 就是正在运行的实体，根据这个设置一些标志位，比如将 A 的 on_rq 设置为 1，而实际正在运行的进程为 B.而 A 可能是 B 的父亲或者祖父.  

正在运行的 se 肯定是对应一个进程的，同时，它不仅需要更新自己的状态，还需要递归地向上，对 parent 的状态进行更新.对于不支持组调度的内核，for_each_sched_entity(se) 等于 se.

再来看 for_each_sched_entity 的源码，就不难理解了:

scheduler_tick->task_tick_fair->for_each_sched_entity
```c++
#ifdef CONFIG_FAIR_GROUP_SCHED
#define for_each_sched_entity(se) \
                for (; se; se = se->parent)
#else
#define for_each_sched_entity(se) \
                for (; se; se = NULL)
#endif
```


接着往下看，task_tick_fair 中调用了 entity_tick:

scheduler_tick->task_tick_fair->entity_tick
```c++
static void
entity_tick(struct cfs_rq *cfs_rq， struct sched_entity *curr， int queued)
{
	update_curr(cfs_rq);

    ...

	if (cfs_rq->nr_running > 1)
		check_preempt_tick(cfs_rq， curr);
}
```

除去负载计算，带宽控制和高精度 tick 相关的代码，调度相关的核心逻辑就只有两部分:



* 更新当前实体以及 parent(使能组调度的情况) 的时间以及状态信息，主要由 update_curr 函数完成.    
* 检查是否需要重新调度，主要由 check_preempt_tick 函数完成.  



#### 更新 vruntime

正如[cfs调度器简介](https://zhuanlan.zhihu.com/p/363784539)中介绍的 cfs 调度算法， cfs 调度器是基于调度实体的虚拟时间 vruntime 进行调度，因此，  update_curr 主要是对当前实体的 vruntime 进行更新:

scheduler_tick->task_tick_fair->entity_tick->update_curr
```c++
static void update_curr(struct cfs_rq *cfs_rq)
{
	struct sched_entity *curr = cfs_rq->curr;

    // rq_clock_task 直接返回 rq->clock_task 变量的值，也就是 rq 上进程运行的总时间
	u64 now = rq_clock_task(rq_of(cfs_rq));
	u64 delta_exec;

    // exec_start 成员是上次统计时的时间点，通过计算与 now 的差值获取本次 task 的执行时间，保存在 delta_exec中.
	delta_exec = now - curr->exec_start;
	if (unlikely((s64)delta_exec <= 0))
		return;
        
    // 重新赋值 exec_start 为当前时间. 
	curr->exec_start = now;

    // 更新统计信息，记住一次运行最长的时间
	schedstat_set(curr->statistics.exec_max，
		      max(delta_exec， curr->statistics.exec_max));

    // sum_exec_runtime 表示从投入运行到现在总共运行的时间
	curr->sum_exec_runtime += delta_exec;

    // exec_clock 表示当前 cfs_rq 占用 CPU 的时间
	schedstat_add(cfs_rq->exec_clock， delta_exec);

    // 这两个函数为核心部分，下面统一分析
	curr->vruntime += calc_delta_fair(delta_exec， curr);
	update_min_vruntime(cfs_rq);

	...
}
```

update_curr 用于更新当前实体的时间状态信息，不仅仅是 entity_tick ，它在很多情况下会被调用，主要执行的逻辑为:



* 获取当前 rq 的时间 clock_task，计算出当前实体本次的运行时间，并以此来更新一些标志位信息
* 更新 vruntime 和 cfs_rq->min_vruntime，鉴于这两项的重要性，有必要单独拿出来聊一聊. 



已知 delta_exec 是当前实体本次运行的实际时间，根据在[cfs调度器简介](https://zhuanlan.zhihu.com/p/363784539)对 vruntime 和 min_vruntime 的描述，可以知道，一个进程的优先级越高，其对应的调度实体的 vruntime 增长速度越慢，nice 值为 0 的进程对应的增长速度和实际时间是一致的.将 delta_exec 作为参数传入到 calc_delta_fair 函数中，将返回当前实体应该增长的 vruntime 的值，附加到 curr->vruntime 上.   

具体的计算过程在 calc_delta_fair 中，这个函数在后续的文章中将会详细讨论.

另一个比较重要的部分就是 cfs_rq min_vruntime 的更新了，这个变量相当于 cfs_rq 中的一个基准线，通常它比整个 cfs_rq 上所有调度实体的 vruntime 都要小(但并不绝对)，所有新加入的进程的 vruntime 都是基于 min_vruntime 的值进行更新. update_min_vruntime 函数的逻辑为:

1.当前执行的进程存在于该 cfs_rq 上，min_vruntime = curr->vruntime
2.当前执行的进程不存在于该 cfs_rq 上，可能是更新其它 cfs_rq，那就赋值为 left_most->vruntime
3.和 clock 的设置一样，谨慎地检查 vruntime 是否大于当前的 min_vruntime，防止 min_vruntime 的回退. 




#### 检查调度
更新完 vruntime，就需要考虑另外一件事:检查当前 cfs_rq 上是否需要调度，对应的接口为 check_preempt_tick.

scheduler_tick->task_tick_fair->entity_tick->check_preempt_tick
```c++
static void
check_preempt_tick(struct cfs_rq *cfs_rq， struct sched_entity *curr)
{
	unsigned long ideal_runtime， delta_exec;
	struct sched_entity *se;
	s64 delta;
    // 计算出在一个周期内，该 se 应该运行的时间，详见下文
	ideal_runtime = sched_slice(cfs_rq， curr);

    // 计算本次进程投入运行以来执行的时间，注意:这里的 delta_exec 和 update_curr 中计算得出的 delta_exec 是两个不同的概念，update_curr 中的 delta_exec 是两次统计之间的间隔，而这里是当前进程运行以来的时间，和统计次数无关. 
	delta_exec = curr->sum_exec_runtime - curr->prev_sum_exec_runtime;

    // 如果运行时间大于 task 应该运行的时间，则设置抢占标志，表示当前进程需要被抢占.  
	if (delta_exec > ideal_runtime) {
		resched_curr(rq_of(cfs_rq));
		clear_buddies(cfs_rq， curr);
		return;
	}

    // 如果运行时间小于预设的进程运行最小值，不执行调度
	if (delta_exec < sysctl_sched_min_granularity)
		return;

    // 在 cfs_rq 中选择 vruntime 值最小的进程，也就是 leftmost 进程.
	se = __pick_first_entity(cfs_rq);

    // 计算两个进程 vruntime 之间的差值 delta.  
	delta = curr->vruntime - se->vruntime;

	if (delta < 0)
		return;
    // 当差值大于进程应该运行的时间，那么当前进程就应该被调度.  
	if (delta > ideal_runtime)
		resched_curr(rq_of(cfs_rq));
}
```

如果你仔细阅读上面的代码，就会发现一些奇怪的地方，也就是与 cfs 调度算法的理论不那么符合的部分，我们一一来看:

在函数之初，会使用 sched_slice 函数来计算一个进程应该运行的时间，如何计算呢?   

内核中预定义了两个相关变量:sysctl_sched_latency 和 sysctl_sched_min_granularity，即调度延迟和进程最小执行时间，调度延迟表示 root cfs_rq 上所有进程运行一次所需要的时间，这个值通常被设置为 6ms 或 12ms 或者其它相近的值，但是一旦系统中进程过多，可能导致每个进程分到的执行时间太短，因此当大于 sched_nr_latency(通常设置为8) 个进程实体时，这个值就会线性地扩展.  

而进程最小执行时间通常被设置为 sysctl_sched_latency/sched_nr_latency，0.75ms 或者 1.5ms.  

sched_slice 函数计算一个进程实体在一个调度延迟中应该执行的时间，其实就是通过其权重与总体权重的对比计算得到的结果，返回值保存在 idle_time 变量中.  

好了，了解了 idle_time 的由来，再来重新审视上面的代码，一共分为三个部分:



* 当进程运行时间超过 idle_time 时，执行重新调度，但是我们需要考虑的是:检查是否需要重新调度除了在 schedule_tick 中会被调用之外，还会在少数的几个地方调用到，比如新进程创建，进程唤醒，迁移等，算不上频繁，比如在一个 HZ 为 100 的系统中，tick 的触发周期为 10ms，当系统不是非常繁忙时，没有出现其它的检查是否需要重新调度的点，可能一个进程实体计算得到的 idle_time 为 2ms，但是它会持续执行完一个 tick 周期 10ms，因为没有谁会打断它，这时候其它进程执行时间也会成比例地拉长，调度延迟在这种情况下并不起作用，或者说在大多数情况下它都不是绝对准确的，因此，在我看来，这个调度延迟或者说调度周期只是在繁忙的系统中的一个参考值，不能作为调度依据.  
* 如果一个进程执行不到最小进程执行时间，不执行重新调度，这是 check_preempt_tick 中的第二个判断，我们能不能理解为在 cfs 调度器中，一个进程如果不主动睡眠，会至少运行 sysctl_sched_min_granularity 设定的最小时间呢? 这并不一定.从上述的源代码来看，sysctl_sched_latency = sysctl_sched_min_granularity * sched_nr_latency，当系统中刚好 sched_nr_latency 个进程时，平均每个进程分到的时间为 sysctl_sched_min_granularity，这时候再考虑上优先级，那么优先级低的进程分到的时间会不到 sysctl_sched_min_granularity，再参考上一个部分(超过idle_time 被切出)可以得出，一个进程实体，是可能出现运行不足 sysctl_sched_min_granularity 就执行切换的.  
* 计算当前 cfs_rq 上 vruntime 最小的进程与当前进程 vruntime 的差值，如果差值大于 ideal_runtime，则执行调度. 在刚看到 if (delta > ideal_runtime) 这个判断的时候，就觉得非常奇怪，因为按照理论上来说，这里应该是  if (delta > 0)，也就是当 curr->vruntime 大于红黑树上的 leftmost->vruntime 时，就执行调度，这才符合 cfs 的策略：始终让 vruntime 最小的进程执行。但实际上明显不是这样，而是让当前运行进程至少比 leftmost 高出一个 idle_time 才会让出 CPU，这种做法倒是体现出了不同优先级之间的区别，因为不同优先级的 idle_time 是不同的，低优先级的进程更容易让出 CPU。当然，它的坏处在于让调度时间变得更不可预测，但是经过我们上面的两部分分析，可以知道，调度行为在一定程度上本来就是不可预测的，这种做法到底是不是更好呢？至于实际效果如何，只有经过大量的测试才知道，至少目前来看(依旧存在在内核中)，它的效果应该是不错的。由于 cfs 调度器实现的是全局的公平性，因此不那么理想化的 delta > ideal_runtime 可能会更好，因为判断 delta > 0 可能会带来进程切换太过频繁的问题(这一段属个人猜测)。 

在分析了 cfs 周期调度相关的代码之后，发现其实 cfs 调度器并没有想象中那么优雅，至少并不像理想中的那样"规整"，也存在着一些修修补补，不过，实际的环境毕竟不是学术研究，优雅的实现固然重要，实际的效果才是最终追求。   

实际上，cfs 的设计优雅之处并不在于实现局部的公平，而是在于全局的公平性，正如上面所分析的，cfs 调度器管理的进程可以不严格遵循调度延迟的规则，也可以不遵循最小进程执行时间的限制，甚至当前在运行的进程很可能不是 vruntime 最小的进程，但是，它的虚拟时间设计总是可以让所有进程之间的执行达到动态的平衡，而且其实现还非常简单，不得不感慨算法之美。  



### 参考

https://www.cnblogs.com/linhaostudy/p/9867364.html

https://zhuanlan.zhihu.com/p/163728119

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。

