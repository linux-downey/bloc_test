# linux调度子系统13 -  负载均衡基本概念
在前面的章节中，介绍了 CPU 拓扑结构以及 CPU 的初始化处理，CPU 的拓扑结构被保存在 cpu_topology 这个结构体数组中，包括 CPU 对应的 cluster_id，core_id 等，对于内核而言，CPU 和内存一样，也被看成一项硬件资源，它被动地接收调度器的安排，负责从内存中取出指令、执行指令、必要的时候将执行结果写回到内存。  

在单核系统中，只有一个逻辑 CPU，针对 CPU 的优化就是在有工作需要处理时尽量地不让它停下来，比如在 CPU 等待资源的时候切换到其它工作。在多核中的逻辑也是一样的，比如在一个简单的双核系统中，目标就是尽量地让两个核都不要停下来，除了单核下的那些优化方法之外，还涉及到另一个问题：如何让工作平均地分配到每个核上。这里所说的平均其实也是相对的，因为在异构多核下，核与核之间的功能是有差别的，绝对的平均反而不适合。

系统中程序的最小执行单位为线程，也就是说调度器会将线程或者进程分配到 CPU 上执行，实际上，一个进程从诞生开始，通常都会在睡眠与唤醒之间来回切换，调度器无法预测哪些 CPU 上的进程将会睡眠而什么时候会被唤醒，可能某个 CPU 上的进程已经全部陷入睡眠而其它 CPU 上不堪重负，因此就涉及到一个优化问题：CPU 之间任务的负载均衡，也就是动态地将繁忙 CPU 上的任务迁移到相对空闲的 CPU 上。  

这个迁移的过程涉及到几个问题：



* 什么时候需要迁移？迁移发生在某些特定的场景下，比如 tick 中断、CPU 空闲时。 
* 如何确定哪个 CPU 繁忙而哪个 CPU 空闲？这个统计由调度器完成，调度器可以实时地记录 CPU 对应就绪队列上的负载。  
* 如何确定需要被迁移的任务？找到最繁忙的 CPU 的就绪队列和最空闲的就绪队列，从就绪队列上找到需要迁移的进程。
* 如何进行迁移？一旦确定了要迁移的任务，就直接将任务从源 CPU 的就绪队列上摘下来，放到目标就绪队列上。    

从理论上来说，这三点都是比较简单的，但是对于实际的实现，又涉及到很多非常繁琐的细节处理，比如繁忙和空闲的程度要达到一个什么样的比例才需要迁移？哪些进程不能被迁移？等等.  

在本章中，我们将重点讨论上文中的第一三四点，即确定迁移的进程以及迁移的具体操作，对于第二点 CPU 负载的计算并不详细讨论。  




## 什么时候触发负载均衡?
负载均衡的触发实际上是非常频繁的，在 tick 中断中会尝试执行负载均衡，也就是内核会周期性地检查 CPU 之间的进程间是否平衡，tick 中断的周期由全局变量 HZ 决定，通常是 10ms 或者 4ms，对应的源码如下：

```c++
void scheduler_tick(void)
{
	...
	trigger_load_balance(rq);
}

void trigger_load_balance(struct rq *rq)
{
	if (time_after_eq(jiffies， rq->next_balance))
		raise_softirq(SCHED_SOFTIRQ);
}

```
执行负载均衡这项操作需要保证一定的间隔时间，太过于频繁地执行负载均衡其实是没有什么意义的，因此 CPU 的 rq->next_balance 用来设置下一次执行负载均衡的时间，这个时间是由调度域设置的，可以通过 /proc/sys/kernel/sched_domain/cpux/domainx/min_interval 和 max_interval 查看对应调度域设置的负载均衡最小和最大间隔时间，比如我的 ubuntu18 系统上的时间就是 2ms/4ms.  

当时间达到 rq->next_balance 时，将会触发软中断，执行负载均衡操作.  

同时，在一个 CPU 上没有可执行的进程时，也就是即将调度执行 idle 进程时，同样将触发负载均衡，这并不难理解，CPU 自己没工作需要执行了，自然要去别的 CPU 上看看有没有多余的工作可以转移过来，对应的内核函数为 idle_balance.  

上述的负载均衡执行的是整体上的负载均衡，而下面几种情况则是为即将运行的进程选择合适的 CPU，对系统的负载均衡有所贡献，但是并不算严格的负载均衡：



* 在执行 fork 创建子进程时，子进程被第一次调度运行时，将会选择一个合适的 CPU 执行。 
* 在进程发起 execve 系统调用时，将会选择一个合适的 CPU 执行。 
* 当进程被唤醒时，同样会选择合适的 CPU 执行。 

上面只是提到了负载均衡触发的时机以及对应的接口，其具体的源码实现我们将在后面讨论.   



## CPU 调度域
在 CPU 的负载均衡中有一个非常重要的概念:CPU 的调度域，在 [cpu 拓扑结构](https://zhuanlan.zhihu.com/p/363799831)中，介绍了 CPU 的拓扑结构以及缓存相关的知识，对于一个复杂的系统而言， CPU 的拓扑结构层次为 NUMA->socket(物理CPU)->cores->SMT，为什么分成这样的层次在该章节中有相应的介绍，从调度层面来看，同一个区域内的逻辑CPU之间迁移任务的开销是最小的，主要是出于缓存的考虑.   

对于同一个核心上的不同硬件线程来说，它们的大多数硬件资源都是共享的，core 内 core 外的所有级别的 cache 自然也是共享的，因此，将一个任务迁移到相同 core 的另一个硬件线程上，成本非常低.  

如果将任务在同一个 socket 中的不同 core 之间迁移，core 内的 cache(L1 为 per core，L2取决于处理器实现)是不共享的，而上层的 unify cache 和内存是共享的，将这些进程进行迁移的时候 core 内的 cache 需要重新填充，有一定的成本.  

实际上，NUMA 结构的实现并不只是一层，因为不想过多讨论 NUMA，因此我把它的概念简化了，它至少是有两部分的:一个 NUMA 系统包含多个 NUMA 节点，而一个 NUMA 节点也可以包含多个 socket，因此，跨 socket 的逻辑 CPU 之间的进程迁移只会共享本地内存或者 unify 内存部分，跨 NUMA 节点的进程迁移，可以想到，只共享最后一层 unify 内存.当然，迁移成本也是逐渐升高的.  

同时，上述所描述的 CPU 拓扑结构其实只是一个通用的概念，具体是什么结构取决于厂商的实现，而实现根据应用场景而定，通常是非常灵活的，比如多层次的大小核，非对称的物理 CPU 等等.我们所讨论的只是一种普遍情况，具体情况还需要参考对应的手册.   



### sched_domain
既然不同的 CPU 层级中进程迁移的成本是有区别的，这种区别自然也会在软件中有所体现，内核中使用 sched_domain 来描述调度域，调度域的抽象和硬件上的层级是类似的，不同的调度域层级中存在父子关系，由 sched_domain 中的 parent 和 child 指针建立父级的索引关系.  

同一个调度域内，所有同层级成员拥有相同的属性，比如一个 socket 中存在多个 core， 在 socket 调度域中，这些 core 共享 L3 cache(以及可能的 L2) 和内存资源，而在 core 所属的调度域内，所包含的硬件线程共享所有 level 的 cache 和内存，划分调度域的作用在于:在执行负载均衡时，以调度域为单位进行进程迁移，优先在调度域内处理，再依次向上处理高层级调度域，可以降低迁移的成本.同时也降低了软件的复杂度.    



### sched_group 
调度域的概念比较好理解，直接参考 CPU 的拓扑结构即可，但是在实际的负载均衡操作中，使用地更多的概念是调度组，有了调度域来区分 CPU 的层级为什么还需要调度组?   

组织了半天语言，发现没法儿通过文字的方式比较精确地表达 sched_domain 和 sched_group 之间的关系，还是通过示例和图来解释吧.  

假设在一个系统中，存在两个 socket，每个 socket 包含 4 个核心，而每个核心包含两个硬件线程，sched_group 和 sched_domain 的关系见下图：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/schedule/sched_domain%E5%92%8Csched_group%E5%85%B3%E7%B3%BB.png)

从图中可以看出，因为包含两个 socket，因此顶层的 domain 包含两个子 domain，每个子 domain 对应一个 group，group 以循环链表的方式组织起来，sched_domain->groups 指向第一个 group。  

每个 group 的 cpumask 字段保存了子 domain 包含的所有 CPU，因此，在 Level3 domain 的 Grp0 包含 CPU0~cpu7 共 8 个逻辑 CPU，而 Grp1 类似。  

在 L2 domain 中，每个 socket 包含四个 core，也就是四个子 domain，由四个组表示，每个组包含两个逻辑 CPU，这里的逻辑 CPU 实际上是一个 core 上的两个硬件线程，依次类推。  

**实际上，由于空间的原因，上图只是展示了 domain 与 group 在系统中的总体关系，这并不对应数据结构中记录的结构，具体情况如下：**



* **CPU domain 和 group 在内核中对应的数据结构为 sched_domain 和 sched_group，需要注意的是，sched_domain 是 percpu 的，在上图中每个调度域拥有多个子 domain，但实际上每个 CPU 只会记录当前 CPU 所属的调度域信息，在本 CPU 上，每个 domain 只存在一个 parent 和 child，也就是每个 CPU 只保存了和自身相关的 domain 树状结构的某一条分支。再次强调！上图只是从系统的角度描述的调度域与调度组的情况，但是实际上并不存在对应的软件数据结构，这些 sched domain 和 sched group 信息是被每个 CPU 分开记录的。**   
* 在 L1 domain 中，由于空间原因将 Grp 信息省略了，实际上 Grp0/1 和 L2 domain 中一样，只是 L1 domain 中包含两个 group，每个 group 只包含一个逻辑 CPU。   

由此可知，domain 关注的是层次关系，而 group 则是对 CPU 的一种组织，在做系统的负载均衡时，domain 只是限定了负载均衡的区域，而 group 包含了需要处理的目标 CPU 集合，因此负载均衡中寻找最忙的 CPU 的操作通常是这样的:

```c++
for_each_domain(this_cpu， sd) {
	...
	group = find_busiest_group(&env);
	busiest_queue = find_busiest_queue(&env， group);
	...
}
```
从当前调度域向上递归到每个父级调度域，在每个调度域中，找到最忙的组，在组中找到最忙的 runqueue，也就是最忙的 CPU.  

理解了调度域和调度组之间的联系以及工作方式，负载均衡的难点几乎就已经解决了一半.  



## 选择合适的 CPU 执行进程
当一个进程新创建，调用 exec 或者被唤醒时，通常不需要考虑 cache 的问题(唤醒进程可能会有cache的残留，但概率不大)，因此这时候将进程迁移到其它 CPU 并没有太多影响，选择合适的 CPU 通过 select_task_rq 函数，该函数会调用对应 sched_class->select_task_rq 回调函数，找到一个合适的 CPU 执行，最主要的影响因素是 CPU 的亲和性设置，其次再是取决于 CPU 的繁忙程度.  

对于经历睡眠然后被唤醒的进程而言，如果睡眠时间较短或者 CPU 并不太繁忙，是有可能该进程对应 CPU 的 cache 还没有被刷掉的，因此，如果是睡眠唤醒，需要考虑这个问题，也就是如果唤醒 CPU 与被唤醒进程之前所在的 CPU 在同一个低层的 domain 内(通常所有 CPU 是在同一个 top domain 内的，因此高层的 domain 意义不大)，尽量在 domain 内选择 CPU，毕竟同一个 domain 内共享更多的 cache，然后再找到一个 idlest 的 CPU 来执行唤醒进程的.   

对于创建的和执行 exec 的进程，那就完全不需要考虑 cache 的问题了，毕竟这种进程都是全新的，不会在任何 CPU 上留下 cache 的足迹，因此，对于执行 CPU 的选择，就相对简单一点，不过也有需要注意的一点是:新执行的进程尽量和唤醒进程离得近一些，也就是能在执行唤醒进程低层的 domain 中找到 idlest cpu 就优先使用低层 domain，这样同样是出于 cache 的考虑，毕竟执行这些代码也会产生 cache 的替换，越低层 domain 之间 CPU 的操作就越可能产生可重用的 cache.   



## 负载均衡的执行
正如上文中提到的，系统中将在 tick 中断或者 CPU 无有效进程运行时触发负载均衡.  

需要注意的是，tick 中断的执行是 percpu 的，也就是说每个 CPU 是独立执行 tick 中断的，在执行负载均衡的时候也只会在当前 CPU 处于比较空闲的状态下执行，然后在各级 domain 中找到最 busy 的 CPU，当然，如果当前 CPU 并不空闲，也就没有必要执行负载均衡，那么，为什么这种负载均衡的操作不是在系统中找到最空闲的 CPU 和最 busy 的 CPU，然后直接操作对应的两个 CPU 呢?一方面，每个 CPU 都有机会自行进行负载均衡，另一方面，操作其它 CPU 很容易造成 rq lock 之间的竞争，同时产生一些无效的 cache，对系统来说是不利的，所以，对于负载均衡而言，都是各人自扫门前雪(存在特殊情况，见下文).   

当然，对于即将执行 idle 进程的 CPU 而言，已经确定了当前 CPU 即将处于空闲状态，需要做的就是找到最忙的 CPU 然后把任务迁移过来.  

确定了需要迁移的进程之后就会执行迁移，执行进程的迁移也需要考虑一些额外的细节，并不是所有情况下的进程都可以直接进行迁移:



* 如果这个进程正在 CPU 上执行，自然是不能迁移的(特殊情况有例外).
* 某些进程有 CPU 亲和性设置或者禁止了某些 CPU，进程迁移不能逾越这个规矩.  
* 如果进程即将被移出就绪队列，执行进程迁移也没什么意义，cfs 调度组的带宽控制会产生这个条件.  
* 如果进程的 cache 是热的，也不建议迁移，这并不是硬性限制，在进程迁移失败多次后可能会将其迁移.  

在找到可以迁移的进程之后，情况就变得比较简单了，直接对就绪队列上的进程执行 dequeue，然后 enqueue 到空闲 CPU 的队列上，也就完成了进程的迁移，当然，也有可能进程迁移失败，可能是系统中本来就没几个可迁移的进程，也可能是由于 CPU 亲和性的设置导致某个 CPU 被刻意地孤立，总之，负载均衡的工作到这里算是完成了.   





---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。