# linux调度子系统1 - 就绪队列
就绪队列，顾名思义，就是用来记录就绪进程的队列，队列这个名称只是遗留下来的一个称呼，它的具体实现可能是其它数据结构，可能是链表或者红黑树。   

在多核环境下，每个 CPU(准确地说是一个核心) 可以执行一个单独的执行流，可能是某个进程或者线程，在早期的 2.4 内核版本中，所有 CPU 共享同一个就绪队列，使用锁在多核之间进行同步，可想而知，这是非常低效的，一方面，多核之间的共享数据将会带来大量的缓存失效，另一方面，一个繁忙的系统中将会产生频繁的数据竞争，从而造成 CPU 无谓的等待。   

从 2.6 开始，调度器的修改同时也增加了对多核的支持，每个 CPU 使用单独的就绪队列，可想而知，系统中的进程是共享的，对于一个指定的进程，它可能会被分配到任意一个 CPU 上执行，这取决于软件设计的策略，这种做法明显提高了执行效率，同时也带来一些调度上的复杂性，毕竟需要多花一些心思来处理多核之间进程的平衡，不过相对于 2.4 的共享队列调度而言，所提升的效率非常可观，其带来的复杂性不值一提。  

2.6 对调度另一个不得不提的优化就是实现了调度的模块化，将不同的调度器进行分离，使用调度器类进行抽象，于是内核中的调度策略被分成两部分，一部分是包含了 schedule()、load_balance、scheduler_tick() 的核心调度部分，这一部分是与进程无关特性的抽象，另一部分就是针对不同场景实现的调度器类，比如针对实时进程的 rt、dl 调度器，针对非实时进程的 cfs 调度器。  

比如，当需要选择下一个待运行进程时，核心调度部分只需要调用 pick_next_task()，然后获取返回的 task ，切换到对应的 task 即可，而不同的调度器类都可以根据自身的调度策略实现自己的 pick_next_task() 接口，返回下一个待运行进程，核心调度部分不需要知道调度器的具体实现，它只需要定下相应的策略。 

这种模块化的设计可以实现非常方便的扩展，不需要修改核心调度器部分代码。比如我想实现一个随机调度器，每次调度都随机选中一个进程运行，只需要根据你指定的调度策略来实现 task_tick、enqueue_task、dequeue_task、pick_next_task 等回调函数即可，注册到系统中，还有一些其它的设置，比如指定新创建进程对应的调度器类为该随机调度器，你就得到一个随机调度系统，不过这个系统运行起来可能会让你非常恼火。  

核心调度部分与与调度器类关系为：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/schedule/%E6%A0%B8%E5%BF%83%E8%B0%83%E5%BA%A6%E9%83%A8%E5%88%86%E4%B8%8E%E8%B0%83%E5%BA%A6%E5%99%A8%E7%B1%BB%E5%85%B3%E7%B3%BB.png)

 




## struct rq 的子就绪队列
调度模块化实现的结果是:per CPU 的运行队列(以下统称为runqueue)不再直接管理进程的调度，而是维护多个子调度器，每个子调度器维护自己的就绪队列，CPU 的rq 负责对这些调度类(对子调度器的抽象)进行管理.当需要执行调度工作时，将根据实际的情况选择子调度器进行处理.    

这样说可能有些抽象，举一个实际的例子:runqueue 其中包含 rt(实时进程调度器) 和 cfs 两个调度类(以这两种为例)，当一个进程被设置了 need_resched 抢占标志时，需要执行调度，这时应该选择下一个合适的进程运行，核心调度部分将会优先从 rq->rt_rq 上查找是否存在实时进程，如果存在，就调用实时进程调度器类的相关接口，按照实时进程的调度策略设置下一个待运行的进程，如果不存在实时进程，就调用 cfs 调度器类的相关接口，以 cfs 的调度策略来选取下一个待运行的进程，毕竟实时进程相对于非实时进程是绝对优先的.当然，这只是一种基本策略，具体实现还涉及到一些优化处理，这部分将在后面的文章讨论.  

关于内核中 CPU runqueue 与各个子调度类的关系，可以参考下图:

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/schedule/runqueue%E7%BB%93%E6%9E%84.jpg)

从图中可以看出，每个 CPU 拥有各自的 runqueue，而 runqueue 中维护了各个调度器类的相关信息，包括 cfs_rq，dl_rq，rt_rq.每个不同的调度器类按照优先级排列依次为 stop_sched_class->dl_sched_class->rt_sched_class->fair_sched_class->idle_sched_class，调度器类之间的优先级是绝对的，也就是当高优先级调度器中存在就绪任务时，就不会轮到低优先级调度器中的任务执行，尽管理论上调度类之间的策略是这样，实际上会有一些宽限的处理，比如默认会有一个实时进程在一个 period 中的最长执行时间占比，默认为 95%，也就是当实时进程一直占用 CPU 时，也会强行给非实时任务留出 5% 的执行时间，这个时间可配置.   

对于 stop_sched_class 和 dl_sched_class，在核心调度部分的 pick_next_task 中作为最高优先级调度器类被扫描，其中stop调度器类一般用来停止 CPU，在 SMP 中使用，会在负载迁移或者 CPU 热插拔的时候使用到，这两类比较少见且特殊，这里暂时不讨论.

rt_sched_class 就是实时调度器类，实时进程的优先级范围为 1~99，实时进程分为两种调度策略，SCHED_FIFO 和 SCHED_RR，SCHED_FIFO. SCHED_RR 表示采用时间片轮转的方式进行调度，而 SCHED_FIFO 则表示先到先得的方式调度，且可以一直运行.内核对实时进程给予了相当程度的照顾，包括它可以任意抢占非实时进程，可以执行任意长的时间，因此，对于实时进程，通常都是一些实时性要求很高的任务，且会频繁进入休眠状态，毕竟一个一直运行的实时进程会导致系统问题.

fair_sched_class 调度器类就是 cfs 调度器的抽象，也是我们重点需要分析的对象，普通的非实时进程由该调度器进行管理， cfs 的就绪队列通过红黑树对调度实体进行管理，在引入了组调度之后，调度实体不再单单是进程，也有可能是进程组，cfs 调度器通过虚拟时间对所有调度实体进行管理，对于一个通用的 linux 系统，绝大多数的进程都属于非实时进程，因此 cfs 调度器是内核调度的核心所在.cfs 调度器的解析将在后续的文章展开.  

idle_sched_class，顾名思义，该调度器类只是在系统空闲的时候才会被使用，也就是系统中没有其它就绪进程时，就会调用到 runqueue 中的 idle 进程，通常在即将调用到 idle 进程之前，都会触发负载均衡，看看其它 CPU 上是不是存在过剩的进程可以执行.  




## struct rq
per cpu 的 runqueue 负责整个 CPU 核上的调度行为，由 struct rq 结构体进行描述， 对于 runqueue，我们专注于 cfs 的分析，因此省略一些不必要的数据结构成员:


```
struct rq {
    // 操作 runqueue 时的需要对该数据结构进行保护，避免并发，尤其是在执行进程迁移的时候，涉及到多个 runqueue 的操作，需要特别小心
    raw_spinlock_t lock;

    // 运行队列上调度实体的个数，是所有子调度器类中就绪实体之和
    unsigned int nr_running;

    // CPU 的负载统计值，在 CPU 的调度行为中，需要对 CPU 最近的负载进行统计，了解一个 CPU 上的负载是非常有必要的，可以用作负载均衡，或者是通过过去的负载变化对 CPU 未来的调度行为进行预测，以完成一些特定的需求.   
    #define CPU_LOAD_IDX_MAX 5
	unsigned long cpu_load[CPU_LOAD_IDX_MAX];

    // 这里的 load 不是负载，而是 rq 的权重，对于每个调度实体，都有一个权重值来表示进程的优先级，这里的 load 是整个队列上的总 load 值，反映了当前 runqueue 上进程的总体权重信息. load_weight 包括一个权重值和一个除数，除数的存在是因为内核中不方便做除法，因此使用一些固定的系数将除法变成其它类型计算，这个后续还会再讨论.  
    struct load_weight load;

    // 负载的统计次数
    unsigned long nr_load_updates;

    // 该运行队列上进程的切换次数
    u64 nr_switches;

    // cfs 调度器类的就绪队列
    struct cfs_rq cfs;
    // rt 调度器类的就绪队列
	struct rt_rq rt;
    // dl 调度器类的就绪队列
	struct dl_rq dl;

    // 保存的进程指针，分别对应当前执行进程 curr，idle 进程(空闲时调用)，stop进程(用于停止 CPU)
    struct task_struct *curr， *idle， *stop;

    // 上一次被切换进程的 mm 结构，在内核中，mm 结构被用来描述进程用户空间的内存布局以及映射，保存上一个进程的 mm 主要是因为内核线程不存在 mm 结构，假设内核线程切换回原来的进程，就可以省去 mm 的释放，主要是 TLB 相关的操作，这样可以节省效率，在后面进程切换中我们再消息讨论这一部分.  
    struct mm_struct *prev_mm;

    // 这三个是 runqueue 的时间相关参数，它们单调增长地记录了进程运行的时间，其中 clock 是从调度开始的总时间，而 clock_task 是纯粹的进程所消耗的时间，毕竟中断的执行也会占用系统时间，clock 的事件刻度并不是以 tick 为单位，而是以硬件定时器的刻度进行计时，精度为 ns 级别. 
    unsigned int clock_update_flags;
	u64 clock;
	u64 clock_task;

    // 由于等待 io 而阻塞的进程数量
    atomic_t nr_iowait;

    // 调度相关的统计信息，如果使能了 CONFIG_SCHEDSTATS，会给内核带来一些额外的开销，包括额外的代码和执行时间，这部分主要应用于 debug 使用，用户可以在 /proc/schedstat 下获取到相关的信息.
    #ifdef CONFIG_SCHEDSTATS
        // sched_info 主要是调度时间相关的参数
        struct sched_info rq_sched_info;
        unsigned long long rq_cpu_time;
        

        // 调用 yield 的次数
        unsigned int yld_count;

        // schedule() 的执行统计，以及进入 idle 进程的统计
        unsigned int sched_count;
        unsigned int sched_goidle;

        // 唤醒相关的统计
        unsigned int ttwu_count;
        unsigned int ttwu_local;
    #endif
};
```



### 参考

4.9.88 内核源码

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。



