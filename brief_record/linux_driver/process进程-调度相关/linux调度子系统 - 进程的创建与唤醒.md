# linux调度子系统 - 进程的创建与唤醒
除了核心的周期性调度和主调度器之外，进程创建和进程唤醒时调度相关策略的实现也值得关注，进程的新建是一切的开端，而进程的睡眠唤醒也是非常重要的，毕竟系统中几乎所有进程都是不断地处于 睡眠-唤醒 的循环过程中。  


## 新建进程的调度设置
fork 创建新进程时，大部分资源都是从父进程复制过来的，调度方面也不例外，用户进程的 fork 发起内核中对应的 fork 系统调用，fork 是 _do_fork 的一层封装，传入的 clone_flags 为 SIGCHLD，当然，创建新的进程还有其它的接口，包括 vfork/clone，这几个接口的不同之处在于在于 clone_flags 的不同，对于 vrofk 而言，clone_flags = CLONE_VFORK | CLONE_VM | SIGCHLD，而对于 clone 系统调用，可以自定义设置 flags，clone_flags 决定了进程资源的复制行为.   

需要明确的一个基本概念是:进程在内核中表现为一组资源的集合，在执行 fork 相关函数时，当前执行的进程是父进程，创建一个子进程的行为就是父进程将自身内核中的资源进行复制(主要是 task_struct 所描述的资源)，只是添加少量的修改(比如PID).  


### sched_fork
在 _do_fork 函数中，首先会调用 copy_process 为子进程创建并初始化一个 task_struct 结构，进程描述符中大部分内容都是从父进程复制过来的。其中，对于调度相关参数的复制处理集中在 sched_fork 函数中：

```c++
fork->_do_fork->copy_process->sched_fork:

int sched_fork(unsigned long clone_flags， struct task_struct *p)
{
    ...
    int cpu = get_cpu();
	__sched_fork(clone_flags， p);             ...............................1

    p->state = TASK_NEW;
    p->prio = current->normal_prio;

    if (unlikely(p->sched_reset_on_fork)) {   ................................2
		if (task_has_dl_policy(p) || task_has_rt_policy(p)) {
			p->policy = SCHED_NORMAL;
			p->static_prio = NICE_TO_PRIO(0);
			p->rt_priority = 0;
		} else if (PRIO_TO_NICE(p->static_prio) < 0)
			p->static_prio = NICE_TO_PRIO(0);

		p->prio = p->normal_prio = __normal_prio(p);
		set_load_weight(p);

		p->sched_reset_on_fork = 0;
	}

    if (dl_prio(p->prio)) {                     ..............................3
		put_cpu();
		return -EAGAIN;
	} else if (rt_prio(p->prio)) {
		p->sched_class = &rt_sched_class;
	} else {
		p->sched_class = &fair_sched_class;
	}

    __set_task_cpu(p， cpu);                    ...............................4
	if (p->sched_class->task_fork)
		p->sched_class->task_fork(p);
	raw_spin_unlock_irqrestore(&p->pi_lock， flags);

#if defined(CONFIG_SMP)                        ...............................5
	p->on_cpu = 0;
#endif
	init_task_preempt_count(p);
#ifdef CONFIG_SMP
	plist_node_init(&p->pushable_tasks， MAX_PRIO);
	RB_CLEAR_NODE(&p->pushable_dl_tasks);
#endif

	put_cpu();
}
```

注1: 在 __sched_fork 中，主要是初始化一个 task 相关的 se:

```c++
	p->on_rq			= 0;
	p->se.on_rq			= 0;
	p->se.exec_start		= 0;
	p->se.sum_exec_runtime		= 0;
	p->se.prev_sum_exec_runtime	= 0;
	p->se.nr_migrations		= 0;
	p->se.vruntime			= 0;
	INIT_LIST_HEAD(&p->se.group_node);

#ifdef CONFIG_FAIR_GROUP_SCHED
	p->se.cfs_rq			= NULL;
#endif
	...
```
这里并没有什么特殊的部分，同时还包含一些 rt 相关的初始化和 NUMA 节点的处理，这里暂不讨论.  

接着，设置进程的 state 为 TASK_NEW，这是一种中间状态，可以保证它不会被实际运行，信号或者外部的时间也不能将其设置为 running.  

再设置进程的 prio 为父进程的 normal_prio，这里的 prio 是动态优先级，进程在某些情况下可能会实现优先级的反转，比如 rt_mutex，因此 prio 的值可能会被临时修改，但是这种父进程的优先级修改并不需要传递到子进程中，所以将父进程的 normal_prio 赋值给子进程的 prio.prio 这个变量在 cfs 调度器中并不重要.  


注2 : linux 中对于各类进程，可以设置不同的调度策略，在用户空间可以通过 sched_setscheduler 进程设置，其中 policy 字段表示进程的调度策略，SCHED_RR 和 SCHED_FIFO 代表实时进程，SCHED_NORMAL，SCHED_BATCH 和 SCHED_IDLE 表示普通进程，SCHED_DEADLINE 表示 dl 进程.  

默认情况下，子进程复制父进程的 policy，当父进程是实时进程时，如果我们想要 fork 出一个非实时进程，可以通过 sched_setscheduler 设置 SCHED_FLAG_RESET_ON_FORK 标志，让进程的 policy reset 回非实时进程，默认产生的进程优先级为 NICE 0，对应的 static_prio 为 120，同时根据该 static_prio 设置 p->se 的 load weight.  

注3 : 根据进程的 prio 设置进程对应的调度器类，尽管 prio 在 cfs 调度器中不重要，但是通过它的值可以区分进程属于哪种调度类型的进程. 

注4 : 在多核架构中，选择哪个 CPU 来运行子进程也是一件比较重要的事，对于 fork 过程来说，子进程直接继承了父进程的所属的 CPU，在这个阶段并不会考虑负载均衡，一方面，在子进程后续的设置中可能会修改 cpu_allowed，也就是 cpu 的亲和性，同时，CPU 的热插拔支持也可能导致此处的设置无效，因此选择 CPU 这项工作放在将子进程投入运行的时候，以及进程执行 execve 系统调用时，在子进程调用 exec 时再考虑负载均衡是合理的，同时进程在 exec 之前实际运行的代码很少，这时候执行迁移对应的缓存成本是最小的.  

随后，子进程会根据其所设置的调度器类，调用对应调度器类的 task_fork 回调接口，对于 cfs 调度器类，对应 task_fork_fair:

```c++
fork->_do_fork->copy_process->sched_fork->task_fork_fair:

static void task_fork_fair(struct task_struct *p)
{
	struct cfs_rq *cfs_rq;
	struct sched_entity *se = &p->se， *curr;
	struct rq *rq = this_rq();
	struct rq_flags rf;

	rq_lock(rq， &rf);
	update_rq_clock(rq);

	cfs_rq = task_cfs_rq(current);
	curr = cfs_rq->curr;
	if (curr) {
		update_curr(cfs_rq);
		se->vruntime = curr->vruntime;
	}
	place_entity(cfs_rq， se， 1);

	if (sysctl_sched_child_runs_first && curr && entity_before(curr， se)) {
		swap(curr->vruntime， se->vruntime);
		resched_curr(rq);
	}

	se->vruntime -= cfs_rq->min_vruntime;
	rq_unlock(rq， &rf);
}
```
按照惯例，在操作 rq 时需要对本地 CPU 的 rq 加锁，防止并发问题，对于 task_fork_fair，其主要的思想还是复制父进程相关的 cfs 调度参数，不过多了一些细节:
* 先更新父进程的时间，保证当前对父子进程的操作是基于最新的 timeline 的. 
* 将子进程的 vruntime 设置为父进程的 vruntime.
* 如果系统调度设置了 START_DEBIT 调度特性，即调度器会让新产生的子进程延后执行，以免那些某些恶意进程通过不断地创建子进程试图占据更多的执行权. 
* 判断系统静态定义的 sysctl_sched_child_runs_first 的值，从该名称可以看出，这个变量决定了子进程是不是要先于父进程运行，默认情况下这个变量的值为 0，表示正常情况下父进程先运行，如果为 1 呢，同时还需要满足的条件是子进程的 vruntime 要小于父进程的 vruntime(该函数执行发生调度的情况)，如果条件满足，具体操作是将父进程和子进程的 vruntime 交换，然后设置重新调度标志位，注意这和 vfork 是不一样的，sysctl_sched_child_runs_first 是针对系统的配置，而 vfork 是针对单个进程的设置，而且 vfork 的实现方式是让父进程等待子进程执行完 exec 而不是交换 vruntime 的值.  
	sysctl_sched_child_runs_first 的值可以通过 cat /proc/sys/kernel/sched_child_runs_first 来查看.  
* 将子进程的 vruntime 减去其对应 cfs 的 min_vruntime，因为当前进程实际上还没有添加到就绪队列，只需要保存一个 vruntime 的绝对值，要了解这步操作可以参考 cfs 的enqueue 和 dequeue 篇博客(TODO).  

注5:在多核架构中， task 的 on_cpu 标志标识了当前进程是否正在 CPU 上执行，需要初始化为 0，然后设置 task 的 preempt_count 为 FORK_PREEMPT_COUNT，这个值等于 2，相当于执行了两次 disable_preempt，当前，在该进程真正投入运行的时候，会被设置为正常值，就像 p->state 一样.  




### 新进程的唤醒
在基本的调度参数设置完成之后，就需要将子进程添加到就绪队列中，"fork函数一次调用两次返回"这种说法成立的条件就是子进程在创建之后得到运行.  

新建的进程想要得到运行就得被添加到就绪队列中，_do_fork 中对应的函数为 wake_up_new_task:

```c++
void wake_up_new_task(struct task_struct *p)
{
	struct rq_flags rf;
	struct rq *rq;

	raw_spin_lock_irqsave(&p->pi_lock， rf.flags);
	p->state = TASK_RUNNING;
#ifdef CONFIG_SMP
	__set_task_cpu(p， select_task_rq(p， task_cpu(p)， SD_BALANCE_FORK， 0));      ..........1
#endif
	
	activate_task(rq， p， ENQUEUE_NOCLOCK);    ...............................2
	p->on_rq = TASK_ON_RQ_QUEUED;
	
	check_preempt_curr(rq， p， WF_FORK);       ................................3
	task_rq_unlock(rq， p， &rf);
}
```
注1 : 在 wake_up_new_task 中，先将子进程的状态设置为 TASK_RUNNING，这是进程投入运行的第一步，同时，在多核架构中，需要为子进程选择一个合适的 CPU 运行，对于不同的调度器类而言，选择合适进程的方式并不一样，尽管这里看起来执行的是通用的 select_task_rq 函数，实际上这个函数根据进程所属的调度器类调用对应的 select_task_rq 回调函数，对于 cfs 调度器而言，调用的是 select_task_rq_fair ，为进程选取合适 CPU runqueue 的过程有些复杂，我们将在后续的进程负载均衡相关的博客中详细讨论.(TODO) 

注2:唤醒新的进程就需要执行入队操作，activate_task 直接调用了 enqueue_task 函数，该函数解析可以参考另一篇博客(TODO)，将子进程添加到 cfs 就绪队列中，同时设置 on_rq 标志为 1，而另一个 on_rq(p->se->on_rq) 在 activate_task 的子函数中被设置.  

注3：check_preempt_curr 函数是用于检查当前正在运行的进程是否需要被指定的进程抢占执行，该函数直接调用对应调度器类的 check_preempt_curr 回调函数，对应 cfs 调度器类的 check_preempt_wakeup，从这个函数名来看，该函数主要用于检查唤醒进程对当前进程的抢占，这里通过传入 WF_FORK 来指示这是对新进程的处理。  

在该函数中，会检查当前进程是否设置了抢占标志，在上文中有提到，如果设置 sysctl_sched_child_runs_first 为 1 且满足一些额外条件，将会对父进程设置抢占标志，而且整个过程中没有出现抢占点，那么这里就会检测到抢占标志而返回。  

该函数接下来判断子进程是否应该抢占当前父进程，同样是使用 wakeup_preempt_entity 函数，这个函数在 schedule() 章节(TODO)分析过，通常传入两个 se，通过两者的优先级判断第二个 se 参数是否应该抢占第一个 se 而获得优先执行权。如果需要执行抢占，就需要对进程设置 TIF_NEED_RESCHED 标志位，在下一个内核抢占点(中断返回或者开抢占的时候)该进程就会被更应该执行的进程抢占。 

到这里，子进程的唤醒就已经完成了，通常子进程将会在父进程不久之后运行，但也并不能保证父进程一定先于子进程运行。  


当使用 vfork 创建进程而不是 fork 时，所传递的 clone_flags 中包含 CLONE_VFORK 标志，当前运行的父进程将会阻塞，直到子进程执行 exec 完成执行程序的切换或者调用 exit 退出，对应的接口为:

```c++
	if (clone_flags & CLONE_VFORK) {
		if (!wait_for_vfork_done(p， &vfork))
			ptrace_event_pid(PTRACE_EVENT_VFORK_DONE， pid);
		}
```



## 唤醒进程的调度设置

进程的唤醒与新进程投入运行的流程其实差不多，只是对于已经运行过而陷入睡眠的进程而言，要唤醒它需要考虑目标进程目前所属的状态以及进程的一些多核的并发处理，对于新进程而言自然不存在这个问题.  

另一方面，新进程通常是被惩罚的对象(和内核配置有关)，而唤醒进程相反，cfs 调度器通常会给唤醒进程一些优待，让其尽快地投入运行，至于唤醒进程是不是抢占当前进程，主要和进程的优先级相关，同时还和内核的配置相关.对应的检查抢占接口为:check_preempt_curr.  



### 进程的唤醒
进程唤醒操作对应的内核底层接口为 try_to_wake_up，因为进程在进入睡眠之后我们必须保留一种方式来找到并唤醒它，因此会有多种类型的上层封装接口，比如 wakeup_process() 或者 wake_up()，最终还是调用了
try_to_wake_up.  

从名称 try_to 可以看出，唤醒工作并不保证一定成功，如果成功，返回 true，代表进程已经被放到就绪队列，而且 state 修改为 RUNNING，但是一个进程即使被唤醒成功，也并不保证它会立即投入运行，只是加入到了就绪队列而已，这点需要注意.  

返回 false 表示唤醒失败.  

对应的源码为:

```c++
static int try_to_wake_up(struct task_struct *p， unsigned int state， int wake_flags)
{
	unsigned long flags;
	int cpu， success = 0;

	...

	cpu = task_cpu(p);

	if (p->on_rq && ttwu_remote(p， wake_flags))        ...........................1
		goto stat;

#ifdef CONFIG_SMP

	p->state = TASK_WAKING;

	cpu = select_task_rq(p， p->wake_cpu， SD_BALANCE_WAKE， wake_flags);  ...................2
	if (task_cpu(p) != cpu) {
		wake_flags |= WF_MIGRATED;
		set_task_cpu(p， cpu);
	}

#endif /* CONFIG_SMP */

	// 注意: ttwu 是 try to wake up 的缩写.
	ttwu_queue(p， cpu， wake_flags);                 ...............................3

	...
}

```

注1:多核的并发向来是最麻烦的问题，它可以做到让一项资源实实在在地被两个 CPU 核同时操作，当然，一个进程不会同时在两个 CPU 核上运行，但是可能出现多个核操作一个进程的情况，CPU0 正在执行进程 A，设置完 A 的 state，然后调用 schedule()，准备将 A 转入睡眠状态，实际上在调用 schedule 之前，是可能发生抢占调度而执行其它进程的，也就是 A 运行状态已经设置，但实际上还没有被真正 dequeue，在其它 CPU 的视野中，A 已经处于费运行状态了.

如果此时 CPU1 发起对 A 的唤醒，当然 CPU1 对 A 的唤醒是有依据的，可能是 A 被添加到了某个唤醒队列中，只是还没执行完 schedule，还没有从队列中完全 dequeue，这时候的唤醒操作就只需要将 A 设置回 TASK_RUNNING 即可，然后检查一下是否需要对目标 CPU 上运行的进程设置抢占标志，当然，这种操作需要竞争 rq lock.  

从函数名 ttwu_remote 也可以看出，这里操作的是其它 CPU 上进程的唤醒，这里的操作比较轻微，因此暂且不用考虑是否共享缓存的问题.  

注2:  和新建进程的唤醒一样，也需要选择当前进程应该运行在哪个 CPU 上，对于睡眠的进程而言，大概率不会占用系统的缓存资源，
因此这是一个不错的负载均衡时机，当然，这里所说的"负载均衡"并不是像周期性调度器中那样的全局性操作，而是针对单个进程的设置，如果该唤醒进程需要进程 CPU 之间的迁移，就设置 WF_MIGRATED 标志位，后续需要针对性的处理.  

注3:这是进程唤醒的实际操作进程，它的实现是这样的:

```c++
try_to_wake_up->ttwu_queue:

static void ttwu_queue(struct task_struct *p， int cpu， int wake_flags)
{
	struct rq *rq = cpu_rq(cpu);
	struct rq_flags rf;

#if defined(CONFIG_SMP)                              ...............................4
	if (sched_feat(TTWU_QUEUE) && !cpus_share_cache(smp_processor_id()， cpu)) {
		sched_clock_cpu(cpu); 
		ttwu_queue_remote(p， cpu， wake_flags);
		return;
	}
#endif
	...
	ttwu_do_activate(rq， p， wake_flags， &rf);           ..............................5
}


```
注1: ttwu_queue 的第二个参数是待唤醒进程应该 enqueue 的 CPU，不一定是本 CPU，当目标 CPU 和当前 CPU 不同享特定 level 的  cache(L1  cache 是 percpu 的，arm 中 L2 是共享的 cache) 且 TTWU_QUEUE 调度特性被支持(默认为 true)时，使用远程唤醒功能，调用 ttwu_queue_remote，这里的唤醒操作和上面的 ttwu_remote 是不一样的，ttwu_remote 执行的代码不多，对缓存的影响不大。

而 ttwu_queue_remote 执行真正的唤醒操作，代码比较多，而且操作的还是目标 CPU rq，将会填充大量 cache，而且这些 cache 对本 CPU 没什么用处，如果不共享 L2 cache 的话，这些 cache 无法复用，对于当前 CPU 和目标 CPU 来说，都是一种浪费.于是折中的做法是调用 ttwu_queue_remote，将当前待唤醒进程添加到目标 CPU 的 wake_list 中，并向目标 CPU 发送一个 IPI 中断，让目标 CPU 来处理进程唤醒的问题，如果目标 CPU 正在执行 idle 进程，就不需要发，因为 idle 进程中会主动扫描其对应 CPU 的 rq->wake_list.这种做法可以提高缓存命中，提升执行效率.  

注2: 从 ttwu_do_activate 可以猜到，这个函数执行的操作就是 activate 待唤醒进程，实际操作就是将待唤醒进程添加到就绪队列中，然后检查抢占.

将唤醒进程添加到就绪队列是由 ttwu_do_activate->activate_task 完成，这个函数将会调用 enqueue_task 将进程添加到就绪队列中，然后设置 p->on_rq 标志位为 1.  

这里有两点需要注意:
* 在调用 ttwu_do_activate 时传入的 flag 参数中会带有 ENQUEUE_WAKEUP 标志，因此在 enqueue_task 中将会调用到 place_entity 函数，该函数会对睡眠的进程实施奖励，实际的操作就是将其 vruntime 值减小，这个值默认是一个调度延迟，即 sysctl_sched_latency，如果启用了 GENTLE_FAIR_SLEEPERS 调度特性，这个奖励值减半.这种做法意味着新唤醒的进程更容易抢占当前执行进程，但是如果睡眠时间并不长，这个奖励也会缩水.这部分可以参考 TODO.   
* 就是如果唤醒的进程是 workqueue 的 worker 内核线程，需要通知 workqueue，好让 workqueue 更合理地安排工作队列的执行.  

将进程添加到就绪队列之后，将调用 ttwu_do_wakeup 函数，对于 cfs 调度器类而言，就仅仅是调用 check_preempt_curr，然后将进程的 state 设置为 TASK_RUNNING，check_preempt_curr 这个函数在前面讲过多次，在这里该函数会判断唤醒进程是否应该抢占当前进程执行，有了 cfs 的奖励加成，高优先级的唤醒进程能够轻而易举地抢占当前进程.  



