# linux调度子系统7 -  se 的 enqueue 和 dequeue
要理解 cfs 调度器，最重要的就是理解其中的三个部分:



* tick 中断的处理，理解在中断中如何对进程的 vruntime 进行更新，以及检查是否需要调度的实现细节
* 进程实体的 enqueue 和 dequeue，了解一个进程在什么情况下会被加入到就绪队列和移出就绪队列，理解 enqueue 和 dequeue 的实现细节，了解这些过程中各个数据结构之间的交互
* pick_next_task，选择下一个将要执行的进程，如何选择下一个进程直接体现了 cfs 的调度策略，同时联系前两个部分，梳理 vruntime 的来龙去脉.  

第一部分的讨论可以[周期性调度](https://zhuanlan.zhihu.com/p/363787529)，本章我们主要来讨论第二部分:cfs 中 enqueue 和 dequeue 的实现与使用.讨论实现主要是基于函数的源码分析，同时还需要在内核中找到它们的调用时机.  

## enqueue_task
需要注意的是，尽管 cfs_rq 使用红黑树对所有 se 进行管理，但这并不是 cfs_rq 的全部，还包括相应数据结构以及标志位的更新.  

因此，进程加入就绪队列并不仅仅是将 se 的 rb 节点添加到红黑树中，还涉及到一系列的操作，包括:



* 更新 vruntime
* 更新负载信息
* 更新 on_rq 标志位
* ...

这些操作的集合构成了内核中的 enqueue_task_fair() 函数.  

实际上，enqueue_task() 是核心调度器实现的函数，用于将进程加入到就绪队列中，在该函数中调用进程对应 sched_class->enqueue_task 回调接口，对应于 cfs 调度器就是 enqueue_task_fair():


```c++
enqueue_task->enqueue_task_fair

static void enqueue_task_fair(struct rq *rq， struct task_struct *p， int flags)
{
    struct cfs_rq *cfs_rq;
	struct sched_entity *se = &p->se;
    ...
	for_each_sched_entity(se) {
        // 判断是否on_rq，见下文
		if (se->on_rq)                      .....................1
			break;
		cfs_rq = cfs_rq_of(se);
		enqueue_entity(cfs_rq， se， flags);  .....................2

        // 组调度的带宽控制，如果配置了带宽控制，在一个周期内只能运行特定的时间，超时则被移出直到下一个周期
		if (cfs_rq_throttled(cfs_rq))
			break;
		cfs_rq->h_nr_running++;
		flags = ENQUEUE_WAKEUP;
	}

	for_each_sched_entity(se) {       ......................3
		cfs_rq = cfs_rq_of(se);
		cfs_rq->h_nr_running++;

		if (cfs_rq_throttled(cfs_rq))
			break;
	}

	if (!se)
		add_nr_running(rq， 1);
    ...
}
```

这里需要注意一个点，就是 on_rq 标志位的设置，在前面的章节中有提到过，当一个进程由就绪态切换到运行态时，该进程会被从红黑树中移除，但是，正如上文所说，红黑树并不代表整个 cfs_rq，也就是即使运行进程不在红黑树上，它依旧在 cfs_rq 上，因此 on_rq 标志位并不会被置为 0.  

因此，一个 state 为 TASK_RUNNING 的进程，不管它处于就绪还是正在运行，对应的 task->on_rq 和 se->on_rq 都还是为 1 的. 

在 enqueue_task_fair 中，使用了 for_each_sched_entity 宏来递归地处理组调度中的情况.当一个组中某个 se 被运行时，这个 group se 应该怎么设置呢?

答案是:基于对调度实体的抽象，在上级的 cfs_rq 的视野中，该 group se 就是正在运行的 se，尽管实际运行的是该 group se 内部的 cfs_rq 上某个 se.因此，group se 的某些标志位也需要被设置成运行态，比如 se->on_rq 标志.  

这种情况就像是:部门领导分配一个任务给组长，在部门领导的视野上来看，我不管这个事是你做还是你分配给其他人做，反正在我的日志上记录的就是你在负责这个事，你怎么完成我不管.  

这种抽象就直接简化了管理流程.  

在 for_each_sched_entity 的递归中，先判断 se->on_rq 是否被置位，如果被置位就跳出向上递归的行为.  

假设有一个调度组 A ，A 中又存在两个实体 se 分别为 B 和 C，如果 C 原本就是就绪的 se，那么 A 也在 cfs_rq 就绪队列中，即 A->on_rq 为 1 .   

当我们要将 B 进行 enqueue 操作时，只需要将 B 添加到 A->cfs_rq 上即可，而如果之前 C 不在就绪队列上，意味着 A 也不在同等级的 cfs_rq 就绪队列上，这时候就需要递归地向上将 B 和 A 都进行 enqueue 操作.  

这也是上面代码中为什么会出现两个 for_each_sched_entity 循环遍历了，第一次向上遍历是为了递归向上将没有添加到就绪队列的 se 以及 se->parent 添加到就绪队列.具体实现为对 se 以及 se->parent 调用 enqueue_entity，同时更新 h_nr_running.当向上遍历遇到的 parent 已经在就绪队列中时，就 break，且此时的 se 已经被赋值为存在于就绪队列中的 parent 了. 

第二次遍历只是为了更新已经存在于就绪队列中的 parent 节点中的相关信息，也就不再需要执行  enqueue_entity 操作，只需要更新 h_nr_running.  

h_nr_running 为 cfs_rq 中所有 se 的数量，递归地统计 group se 中的所有 se，而 nr_running 表示当前 cfs_rq 中第一层的 se 数量，在 enqueue_entity 时，其 se 所属的 cfs_rq->nr_running 和 cfs_rq->h_nr_running 都需要加 1，而其 parent 对应的 cfs_rq 只需要将 h_nr_running 进行加 1 操作，这里只操作了 h_nr_running， nr_running 的操作在 enqueue_entity 这个函数中.  

enqueue_entity 这个函数是将一个实体添加到就绪队列中的实际操作，它的源代码如下:

```c++
enqueue_task->enqueue_task_fair->enqueue_entity:

static void enqueue_entity(struct cfs_rq *cfs_rq， struct sched_entity *se， int flags)
{
    // 判断添加到就绪队列中的原因
	bool renorm = !(flags & ENQUEUE_WAKEUP) || (flags & ENQUEUE_MIGRATED); // ........1
	bool curr = cfs_rq->curr == se;

	if (renorm && curr)
		se->vruntime += cfs_rq->min_vruntime;

	update_curr(cfs_rq);

	if (renorm && !curr)
		se->vruntime += cfs_rq->min_vruntime;

    ...
    /* 负载与组带宽控制相关 */
    ...

    // 更新就绪队列的 load weight 权重
    // 更新 cfs_rq->nr_running
	account_entity_enqueue(cfs_rq， se);

	if (flags & ENQUEUE_WAKEUP)          ...................................2
		place_entity(cfs_rq， se， 0);

	...

	if (!curr)                           ...................................3
		__enqueue_entity(cfs_rq， se);
	se->on_rq = 1;

	...
}

```
注1. 在讨论 enqueue 之前，需要先大概了解一个进程在什么情况下会被 enqueue，这体现在 enqueue_entity 的参数 flags 中，对于 enqueue ，flags 有这么几个参数:



* ENQUEUE_WAKEUP :该进程是从睡眠状态下唤醒的，对应的 dequeue 标志为 DEQUEUE_SLEEP
* ENQUEUE_RESTORE:该标志位表示恢复，通常应用在当用户设置进程的 nice 值或者对进程设置新的调度参数时，为了公平起见，调度器会先把进程 dequeue 然后再 enqueue，改变调度参数的同时应该重新参与调度.对应的 DEQUEUE 标志为 DEQUEUE_SAVE，DEQUEUE_MOVE
* ENQUEUE_MIGRATED:多核架构中，进程迁移时会设置该标志位，表示当前进程是从其它的 rq 上移过来的，需要重新设置 vruntime.

如果一个进程设置了 ENQUEUE_MIGRATED 标志，或者不是从睡眠中被唤醒的，就需要对虚拟时间重新标准化，重新标准化对应的具体操作就是继续参考进程原本自带的 se->vruntime，在此基础上再加上 cfs_rq->min_vruntime，而对于睡眠唤醒且不是迁移的进程而言，之前的 se->vruntime 不再具有参考价值，直接基于 cfs_rq->min_vruntime 设置 se->vruntime，对于特殊情况有一些特殊处理，这部分在下文讨论.   

如果单单只看 enqueue_entity 中如何设置 vruntime，是很容易摸不着头脑的，实际上 enqueue_entity 和 dequeue_entity 是紧密相关的，cfs_rq->min_runtime 通常保持为小于当前 cfs_rq 上所有实体的 se->vruntime，因此，当一个进程从开始运行到被切换为就绪态，它的 se->vruntime 是肯定是大于 cfs_min_vruntime 的.  

想象以下，这时候如果发生进程迁移，由 A 队列到 B 队列，将进程对应的实体从 A 队列中 dequeue，然后 enqueue 到 B 队列中，那么对于 vruntime 的怎么处理呢?

因为每个 cfs_rq 的 min_runtime 并不是同步的，同时 min_runtime 是每个 cfs_rq 的基准值，调度器会先将待迁移进程的 vruntime 减去 A->min_vruntime，留下一个纯粹的属于该进程的 vruntime，然后迁移到 B 时，设置 vruntime 再加上 B->min_vruntime.  

为什么需要保留进程 vruntime 的绝对值(减去 min_vruntime 的值)呢?

因为这个值反映了进程的历史运行情况，如果这个进程刚运行完不久，这个 vruntime 绝对值会比较大，迁移到新的 CPU 上应该也是同样的待遇，把它当成刚运行完的进程，而如果这个进程等了很久了，vruntime 的绝对值较小，迁移到新的 CPU 上会很快得到运行，这也就实现了平滑的迁移，实现了公平性.    

那为什么睡眠的进程不再参考 vruntime 绝对值了呢?

这也很好理解，对于进程 A 而言，从运行态进入睡眠态，该进程将会被 dequeue，此时它的 vruntime 减去 cfs-> min_vruntime 的绝对值相对会较大，当该进程唤醒时，如果再将其绝对值加上 cfs->min_vruntime 作为该进程的 vruntime，也就是将睡眠刚唤醒的进程当成是刚执行完的进程，这明显是不合理的。因此睡眠的进程的 vruntime 就直接被赋值为 min_vruntime 了，不仅这样，cfs 调度算法还会对睡眠进程进行一些奖励.  


注2:判断一个进程是否刚从睡眠中唤醒，在上面的操作中，如果一个进程是被唤醒的而且不是迁移进程，它的 vruntime 还是没有被处理的，而是在 place_entity 中进行处理，这个函数会在两个地方被调用，一个是进程创建时针对新进程的设置，另一个就是这里针对唤醒进程的设置，在目前我阅读的内核版本(4.14)中，会对睡眠进程奖励 sysctl_sched_latency(一个调度延迟) 的时间，但是前提是这个进程的睡眠时间至少超过了这个时间，如果一个进程睡眠 3ms，奖励 6ms，这自然是不合理的.操作方式很简单，就是将 vruntime 减去 sysctl_sched_latency，如果内核中设置了 GENTLE_FAIR_SLEEPERS(平滑补偿) 的调度特性，这个奖励值会被除以 2.  

注3: 如果该进程不是 cfs_rq->curr，就将其添加到红黑树中，对应的红黑树操作函数为 __enqueue_entity，因为 curr 并不需要添加到红黑树中，自然不需要执行这一步操作.



## dequeue_task
理解了 enqueue task ，再来看 dequeue task 就会简单很多，基本上和 enqueue 是相对的.  

同样的，进程被移出队列对应的接口为 dequeue_task，在 dequeue_task 中，也是根据进程的 sched_class 调用对应的 dequeue_task 回调函数，cfs 中则对应 dequeue_task_fair.  

dequeue_task_fair 中几乎完全是 enqueue_task 的相反操作，包括:



* 递归地调用 dequeue_entity，如果当前 se 存在 parent 且该 parent 下不存在其它子 se 在就绪队列上，对 parent 同时调用 dequeue_entity，这是个递归过程，同时递减 h_nr_running. 
* 如果 se-> parent 下还存在其它 se 在就绪队列上，就不需要对 parent 调用 dequeue_entity，而只是更新负载信息，递减 h_nr_running.
* 其它的比如带宽控制，高精度定时器的更新等等，暂不讨论. 

而真正执行移除操作的还是 dequeue_entity 这个函数，同样的，这个函数和 enqueue_entity 是一个相反的操作，包括:



* 先调用 update_curr 更新时间，将 se->vruntime  减去 cfs_rq->min_vruntime，获取一个 vruntime 绝对值保存在 se 中，这一步的目的可以参考上面 enqueue 的解析. 
* 设置 se->on_rq 为0
* 递减 nr_running
* 和 enqueue_entity 不同的地方在于还会调用 clear_buddies，当 cfs_rq 的 last，next，skip 为当前 se 时，将其清除，毕竟都已经被移出 cfs_rq 了，自然要清理干净.  




## 进程入队与出队的时机
enqueue_task 和 dequeue_task 是加入和移出就绪队列的操作实现，一个更重要的问题是，它们会在什么时候被调用，只有了解这个才能从更高的角度看到 cfs 的整体框架.  

在前面章节 [进程状态以及切换](https://zhuanlan.zhihu.com/p/363783778) 中，可以了解到，当一个进程要投入运行，那么它一定是先切换到就绪态，再由调度器来决定什么时候投入运行，切换到就绪态的过程就是 enqueue_task 的过程.  

而一个进程由运行态切换到其它状态时，分两种情况:



* 一种情况是它会被切换回就绪态，这时候并不会发生入队和出队，只是会将进程重新添加到红黑树中，注意，添加到红黑树并不等于 enqueue，这只是红黑树的操作，对应 __enqueue_entity()，而不会影响到其状态信息，比如 on_rq 标志.。在调度讨论中所指的队是 cfs 或者 rq 队列，而红黑树只是 cfs 队列用来管理调度实体的数据结构，这是两个概念。

* 另一种呢，就是切换到其它状态，这时候就涉及到进程的 dequeue，需要调用 dequeue_task.  



除此之外，还存在另一种场景就是负载均衡的时候发生的 CPU 之间的进程迁移，从一个 CPU 的队列上 dequeue，然后在另一个 CPU 的队列上 enqueue.  





---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。