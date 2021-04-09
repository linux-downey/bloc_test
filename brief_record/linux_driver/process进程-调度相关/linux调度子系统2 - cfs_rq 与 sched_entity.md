# linux调度子系统2 - cfs_rq 与 sched_entity
cfs 调度器在内核中被抽象为 fair_sched_class，该调度器类实现了调度器的基本操作，比如最常用的 task_tick (发生tick中断时调度器进行时间更新)、pick_next_task(选择下一个需要运行的进程)、task_fork(在新建进程时设置调度参数)，这些回调函数都作用于 cfs 上的调度实体，调度实体由 sched_entity 进行描述，所有的调度实体都被放在 cfs 的就绪队列上。  

对外部来说，系统的核心调度部分只需要在适当的时机调用目标调度器类的回调函数，返回统一的接口，核心调度部分不关系其内部的实现，实现一个调度器只需要实现核心调度部分所定义的那些接口即可，从而实现模块化。 


## 调度实体
调度实体有两种，一种是 task，另一种是调度组，task 自然好理解，调度器本身就是对进程进行调度，那么调度组应该如何理解？  

组调度的概念是 linux 引入的一种新机制，具体的实现就是对进程在必要的时候进行分组管理，鉴于一些应用场景的需求，比如正由两个用户A、B使用，A 运行1个进程，而 B 运行 9 个进程，如果是以进程为调度对象的情况下，结果是 A 占用系统 10% 的时间，在某些情况下需要将调度器的带宽平均分配给每个用户，而不是谁执行的进程多谁就能占用更多的时间。这时候就完全有必要对进程进行分组，以组为单位进行调度，做到公平调度。  

因此，基于组调度的需求，抽象出的调度实体就包含了 task 和组，在 cfs_rq 的视角中，task 和 组都属于调度实体，它们没有什么不同，同一层的调度实体遵循同样的调度规则。  

但是，一个调度组通常包含多个调度实体，那这些调度实体又该如何分配呢？

答案很简单，就是将 cfs 调度策略递归地进行，每个调度组实体又维护一个 cfs_rq，该组的所有调度实体放在该 cfs_rq 中，同样通过 cfs 调度器类中定义的各种回调函数对组内调度实体进行操作。不同的是，调度组的处理往往需要执行递归操作，对于一些标志位的设定也相对复杂一些。  




## sched_entity
调度实体对应的结构体为 struct sched_entity，该数据结构包含了一个实体执行调度以及运行的所有相关数据，包括在前文中讨论的 vruntime、load 等，下面就是对该结构的分析：

```c++

// 首先需要注意的是，调度实体可能是进程和调度组两种，因此结构体中会同时包含这两类实体相关的数据。 

struct sched_entity {
	// load 表示当前调度实体的权重，这个权重决定了一个调度实体的运行优先级，对进程实体而言，它是由静态优先级计算得到，对应调度组而言，是组内各实体的 load 之和。  
	// load 和 cpu_load 两个名字取得是有歧义的，虽然都是 load，但是 cpu_load 却是表示负载
	struct load_weight		load;
	
	// 红黑树的数据节点，使用该 rb_node 将当前节点挂到红黑树上面，还是内核中的老套路，将 rb_node 嵌入 sched_entity 结构，在操作节点时，可以通过 rb_node 反向获取到其父结构。  	
	struct rb_node			run_node;
	
	// 链表节点，被链接到 percpu 的 rq->cfs_tasks 上，在做 CPU 之间的负载均衡时，就会从该链表上选出 group_node 节点作为迁移进程。  
	struct list_head		group_node;
	
	// 标志位，代表当前调度实体是否在就绪队列上
	unsigned int			on_rq;

	// 当前实体上次被调度执行的时间
	u64				exec_start;
	
	// 当前实体总执行时间
	u64				sum_exec_runtime;
	
	// 截止到上次统计，进程执行的时间，通常，通过 sum_exec_runtime - prev_sum_exec_runtime 来统计进程本次在 CPU 上执行了多长时间，以执行某些时间相关的操作 
	u64				prev_sum_exec_runtime;
	
	// 当前实体的虚拟时间，调度器就是通过调度实体的虚拟时间进行调度，在选择下一个待执行实体时总是选择虚拟时间最小的。
	u64				vruntime;
	
	// 实体执行迁移的次数，在多核系统中，CPU 之间会经常性地执行负载均衡操作，因此调度实体很可能因为负载均衡而迁移到其它 CPU 的就绪队列上。  
	u64				nr_migrations;

	// 进程的属性统计，需要内核配置 CONFIG_SCHEDSTATS，其统计信息包含睡眠统计、等待延迟统计、CPU迁移统计、唤醒统计等。 
	struct sched_statistics		statistics;

#ifdef CONFIG_FAIR_GROUP_SCHED

	// 由于调度实体可能是调度组，调度组中存在嵌套的调度实体，这个标志表示当前实体处于调度组中的深度，当不属于调度组时， depth 为 0.
	int				depth;
	
	
	// 指向父级调度实体
	struct sched_entity		*parent;
	
	// 当前调度实体属于的 cfs_rq.
	struct cfs_rq			*cfs_rq;

	// 如果当前调度实体是一个调度组，那么它将拥有自己的 cfs_rq，属于该组内的所有调度实体在该 cfs_rq 上排列，而且当前 se 也是组内所有调度实体的 parent，子 se 存在一个指针指向 parent，而父级与子 se 的联系并不直接，而是通过访问 cfs_rq 来找到对应的子 se。  
	struct cfs_rq			*my_q;
#endif

#ifdef CONFIG_SMP
	// 在多核系统中，需要记录 CPU 的负载，其统计方式精确到每一个调度实体，而这里的 avg 成员就是用来记录当前实体对于 CPU 的负载贡献。 
	struct sched_avg		avg ____cacheline_aligned_in_smp;
#endif
};

```

对于一个进程调度实体，可以通过 cat /proc/PID/sched 来查看一个调度实体相关的信息，用于调试或者分析使用。 




## struct cfs_rq 结构
struct sched_entity 描述一个调度实体，它包含了一个实体所有运行时的参数，如果要了解各个调度实体是如何管理的，就不得不先了解另一个重要的结构：cfs_rq,即 cfs 调度算法的就绪队列。  


```c++
struct cfs_rq {

	// 在上面的 sched_entity 结构中也存在同样的 load 成员，一个 sched_entity(se) 的 load 成员表示单个 se 的 load，而 cfs_rq 上的 load 表示当前 cfs_rq 上所有实体的 load 总和。
	struct load_weight load;
	
	// 这两个都是对当前 cfs_rq 上实体的统计，区别在于：nr_running 只表示当前 cfs_rq 上存在的子实体，如果子实体是调度组，也只算一个。而 h_nr_running 的统计会递归地包含子调度组中的所有实体。因此可以通过比较这两者是否相等来判断当前 cfs_rq 上是否存在调度组。   
	unsigned int nr_running, h_nr_running;
	
	// 当前 cfs_rq 上执行的时间 
	u64 exec_clock;
	
	// 这是一个非常重要的成员，每个 cfs_rq 都会维护一个最小虚拟时间 min_vruntime，这个虚拟时间是一个基准值，每个新添加到当前队列的 se 都会被初始化为当前的 min_vruntime 附近的值，以保证新添加的执行实体和当前队列上已存在的实体拥有差不多的执行机会，至于执行多长时间，则是由对应实体的 load 决定，该 load 会决定 se->vruntime 的增长速度。  
	u64 min_vruntime;

	// cfs_rq 维护的红黑树结构，其中包含一个根节点以及最左边实体(vruntime最小的实体，对应一个进程)的指针。  
	struct rb_root_cached tasks_timeline;

	/*
	 * 'curr' points to currently running entity on this cfs_rq.
	 * It is set to NULL otherwise (i.e when none are currently running).
	 */
	// 记录当前 cfs_rq 上特殊的几个实体指针：
	// curr：cfs_rq 上当前正在运行的实体，如果运行的进程实体不在当前 cfs_rq 上，设置为 NULL。需要注意的是,在支持组调度的情况下,一个进程 se 运行,被设置为当前 cfs_rq 的 curr,同时其 parent 也会被设置为同级 cfs_rq 的 curr. 
	// next：用户特别指定的需要在下一次调度中执行的进程实体，但是这并不是绝对的，只有在 next 指定的进程实体快要运行(但可能不是下次)的时候，因为这时候不会造成太大的不公平，就会运行指定的 next，也是一种临时提高优先级的做法。 
	// last：上次执行过的实体不应该跨越公平性原则执行，比如将 next 设置为 last，这时候就需要仔细斟酌一下了，也是保证公平性的一种方法。  
	// 
	struct sched_entity *curr, *next, *last, *skip;


#ifdef CONFIG_SMP
	
	// 在多核 CPU 中，对当前 cfs_rq 的负载统计，该统计会精确到每个 se，自然也就会传递到 cfs_rq，下面的几个成员用于负载统计，目前专注于 cfs 调度的主要实现，负载均衡部分后续再进行分析。  
	struct sched_avg avg;
	u64 runnable_load_sum;
	unsigned long runnable_load_avg;
	atomic_long_t removed_load_avg, removed_util_avg;


#ifdef CONFIG_FAIR_GROUP_SCHED
	// 指向 percpu rq 的指针，在不支持组调度的系统中，runqueue 上只存在一个 cfs_rq，可以直接结构体的地址偏移反向获取到 rq 的指针，而支持组调度的 cfs_rq 可能是 root cfs_rq 的子级 cfs_rq,因此需要通过一个指针获取当前 cfs_rq 所在的 rq。  
	struct rq *rq;	

// 这一部分是组调度中的带宽控制，在某些应用场景，比如虚拟化或者用户付费使用服务器中，需要对用户的使用带宽或者时长进行限制，就需要用到 cfs 调度的贷款控制，其实现原理就是在一个周期内，通过某些算法设置用户组应该运行多长时间、同时计算已经运行了多长时间，如果超时，就将该用户组 throttle，夺取其执行权直到下一个周期。同样的，cfs 的带宽控制部分我们暂时不深入讨论。 
#ifdef CONFIG_CFS_BANDWIDTH
	int runtime_enabled;
	int expires_seq;
	u64 runtime_expires;
	s64 runtime_remaining;

	u64 throttled_clock, throttled_clock_task;
	u64 throttled_clock_task_time;
	int throttled, throttle_count;
	struct list_head throttled_list;
#endif /* CONFIG_CFS_BANDWIDTH */
#endif /* CONFIG_FAIR_GROUP_SCHED */
};
```

关于 cfs_rq 与 sched_entity 之间的关系，我们可以参考下图：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/schedule/cfs_rq%E5%92%8Cse%E4%B9%8B%E9%97%B4%E7%9A%84%E5%85%B3%E7%B3%BB.png)



## task 相关调度成员
sched_entity 是对一个调度单元的抽象，实际在 CPU 上执行的还是进程(或线程)，由 task_struct 描述，因此对于真正执行的 task，还需要关注 task_struct 中和调度相关的数据成员,需要注意的是，cfs_rq 和 sched_entity 是 cfs 调度器特定的成员，而 task 是应用于所有调度器的，因此某些调度相关的成员是和其它调度器相关的：

```c++

struct task_struct {

	// 进程目前所处于的状态，0 表示 TASK_RUNNING 状态，虽说是 running 状态，并不代表它一定正在 CPU 上运行，而是两种可能：正在运行或者处于就绪状态。而准确地判断一个进程是否正在运行是通过 on_cpu 字段。  
	// 其它非 0 的值表示其它的非就绪状态：比如睡眠、停止等，具体参考 include/linux/sched.h
	volatile long			state;
	
#ifdef CONFIG_SMP	
	// 判断进程是不是正在某个 CPU 上运行
	int				on_cpu;               
#endif
	
	// 当前进程是否处于就绪队列上，处于就绪队列上时该 flag 为 1，但是有一个特殊情况就是：当非实时进程正在运行时，它会被从就绪队列(红黑树)中暂时移除,但是它的 on_rq 的值还是 1. 
	int				on_rq;
	
	// 该进程的动态优先级，在 cfs 中并不重要。 
	int				prio;
	
	// 进程的静态优先级，该优先级直接决定了非实时进程的 load_weight，从而决定了该进程对应调度实体的 vruntime 增长速度。 
	int				static_prio;
	
	// 对于实时进程和非实时进程而言，优先级以及对应的表达式不一样的，实时进程占用 0~99 号优先级，数字越大优先级越高，而非实时进程则是 100~139，数字越大优先级越低，为了对优先级进行统一，需要将实时优先级经过一层转换，而非实时优先级不需要动，normal_prio 代表的就是统一的优先级表示方法。  
	int				normal_prio;
	
	// 进程的实时优先级，和实时调度相关
	unsigned int			rt_priority;

	// 进程所属的调度器类，也就是由哪种调度器进行管理。 
	const struct sched_class	*sched_class;
	
	// 进程对应的 sched_entity，每一个进程都对应一个 sched_entity，反之并不成立，因为某些 sched_entity 可能对应一个进程组。 
	struct sched_entity		se;
	
	// 实时进程的调度实体，rt_se
	struct sched_rt_entity		rt;
	
#ifdef CONFIG_CGROUP_SCHED
	// 当组调度被使能时，该成员记录了进程所属的调度组。 
	struct task_group		*sched_task_group;
#endif

	// deadline 类调度进程的调度实体 se。
	struct sched_dl_entity		dl;
	
}

```


内核组件的数据结构就已经大体反应了这个组件的内容以及实现，了解其数据结构之后再阅读代码就比较轻松了，之前的文章都是对 cfs 调度器的铺垫以及准备工作，从下一篇开始我们就真正进入调度部分的源码分析了。  








