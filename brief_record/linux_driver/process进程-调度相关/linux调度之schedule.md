# linux 调度之 schedule()
schedule() 是 linux 调度器中最重要的一个函数,就像 fork
函数一样优雅,它没有参数,没有返回值,却实现了内核中最重要的功能,当需要执行实际的调度时,直接调用 shedule(),进程就这样神奇地停止了,而另一个新的进程占据了 CPU.    

在什么情况下需要调度?  
在 scheduler_tick 中,唤醒进程时,重新设置调度参数这些情况下都会检查是否出现抢占调度,但是像 scheduler_tick 中或者中断中唤醒高优先级进程时,这是在中断上下文中,是不能直接执行调度行为的,因此只能设置抢占标志,然后在中断返回的时候再调用 schedule() 函数执行调度.   

上面说的这几种都是被动调度的情况,也就是进程并不是自愿执行调度,而是被抢走了 CPU 执行权,与被动调度相对应的还有主动调度,也就是进程主动放弃 CPU,通常对应的代码为:

```c++
...
set_current_state(TASK_INTERRUPTIBLE);
schedule()
...
```

在 schedle 函数的执行中,current 进程进入睡眠,而一个新的进程被运行,当当前进程被唤醒并得到执行权时,又接着 schedule 后面的代码运行,非常简单的实现,完美地屏蔽了进程切换的内部实现,提供了最简单的接口,这一章我们就来翻开这些被屏蔽的细节,看看 linux 中进程的调度到底是怎么执行的.   

## schedule 源码实现
schedule 函数定义在 kernel/sched/core.c 中,源码如下:

```c++
void __sched schedule(void)
{
	struct task_struct *tsk = current;
    ...
	do {
		preempt_disable();
		__schedule(false);
		sched_preempt_enable_no_resched();
	} while (need_resched());
}
```
schedule() 函数只是个外层的封装,实际调用的还是 __schedule() 函数, __schedule() 接受一个参数,该参数为 bool 型,false 表示非抢占,自愿调度,而 true 则相反.   

在执行整个系统调度的过程中,需要关闭抢占,这也很好理解,内核的抢占本身也是为了执行调度,现在本身就已经在调度了,如果不关抢占,递归地执行进程调度怎么看都是一件没必要的事.  

当然,在调度过程完成之后,也就是 __schedule 返回之后,这个过程中可能会被设置抢占标志,这时候还是需要重新执行调度的.  

但是这里有一个很有意思的事:在执行调度函数 __schedule 的过程中,实际上在某个点已经切换出去了,而当前进程的 __schedule 函数返回的时候其实是当前进程被重新执行的时候,可以简单地理解为,如果发生了实际的进程切换,在 __schedule 函数内实际上经历了一次进程从切出到再次切入,经历的时间可能是几百毫秒或者更长.  

因此,实际上,在当前进程中禁止的抢占,而使能抢占的 sched_preempt_enable_no_resched 函数却是在另一个进程上执行的.好在所有的进程都是通过 schedule() 进行进程切换的,也就保证了 disable 和 enable 总是成对的.    


## __schedule()
__schedule 的实现大概可以分为四个部分：
* 针对当前进程的处理
* 选择下一个需要执行的进程
* 执行切换工作
* 收尾工作

整个 __schedule 函数比较长，也分成这四个部分进行分析。 

### 针对当前进程的处理
static void __schedule(bool preempt)
{
	...
	prev = rq->curr;
	schedule_debug(prev);                        ................................1

	// 禁止本地中断,防止与中断的竞争行为
	local_irq_disable();                        
	...
	// 更新本地 runqueue 的 clock 和 clock_task 变量,这两个变量代表 runqueue 的时间. 
	update_rq_clock(rq);

	switch_count = &prev->nivcsw;                ................................2
	if (!preempt && prev->state) {
		if (unlikely(signal_pending_state(prev->state, prev))) {
			prev->state = TASK_RUNNING;
		} else {
			deactivate_task(rq, prev, DEQUEUE_SLEEP | DEQUEUE_NOCLOCK);
			prev->on_rq = 0;
			...
			if (prev->flags & PF_WQ_WORKER) {
				struct task_struct *to_wakeup;

				to_wakeup = wq_worker_sleeping(prev);
				if (to_wakeup)
					try_to_wake_up_local(to_wakeup, &rf);
			}
		}
		switch_count = &prev->nvcsw;
	}
}

需要注意的是,在 __schedule() 函数的初期,执行了 prev=rq->curr,也就是当前进程被赋值给 prev,因为 __schedule 函数内部会发生一次进程的调度,在调度之前,实际上这个 prev 就是 curr,而在调度回来之后,这个 prev 就是真正的上次执行的进程.  

注1:在 __schedule 调用的初期,会对当前的进程做一些检查,比如 preetion count,试想一下,假设用户执行了这样的代码逻辑:

```c++
...
preempt_disable()
schedule()
...
```

在调度的时候其实并不会出现什么问题,调度器依旧按照原有算法选择一个新进程运行,只是没有对应的 preempt_enable() 被调用,目标进程会一直运行直到良心发现,而且大概率这个 preempt_disable 与 preempt_enable 的失衡会持续存在在系统中.  

当然,除了出于测试目的,开发者不会写出这么明显带有 bug 的代码,实际情况是:preempt_disable 和 schedule 有时候会包含在某些上层接口中,被意外地调用,比如下面的情况:

```c++
执行调用了 preempt_disable 的函数
执行调用了 schedule 的函数
执行调用了 preempt_enable 的函数
```
尽管 preempt_disable 和 preempt_enable 确实是成对出现的,但是中间可能发生了进程切换,比较乐观的后果是系统 crash,更坏的情况是 bug 会一直持续到当前进程再次运行,然后解除 preempt_disable 和 preempt_enable 的不平衡,为什么说这种情况更坏呢?系统 crash 是一个很直接的信号,而后一种情况可能只是导致系统某些时候有些卡,但是能正常运行,解这种 bug 是比较要命的. 

schedule_debug 接口就是对这种行为进行检查,因为在调用 __schedule 之前会调用 preempt_disable,因此这个函数会判断当前 preempt count 是不是 1,如果不是,就会打印相关的警告信息,然后将 preempt 强制修正为正确值,然后继续前行,至于造成这种情况的原因,只能说调度器已经把必要的信息给你了,自己去分析.   

除此之外,还有 rcu 的 sleep check.  

注2:针对当前进程处理的第二部分,主要是针对自愿调度进行处理,通过 if (!preempt && prev->state) 判断,preempt 表示是否是自愿让出,而 prev->state 表示当前进程的状态,0 对应 TASK_RUNNING,正常情况下,进程自愿让出 CPU 都是将 state 设为其它状态.  

进入到 if 判断子代码块中,首先需要先检查该进程是否有未处理信号,如果有,就需要先处理信号,将当前进程的状态设置回 TASK_RUNNING.这种情况下,当前进程会重新参与调度,有很大概率选取到的下一个进程依旧是当前进程,从而不执行实际的进程切换.  

如果没有未处理信号,就调用 deactivate_task() 将进程从队列中移除,实际上就是调用了 dequeue_task 函数,同时将进程的 on_rq 设置为 0,表示当前进程已经不在 CPU 上了(但实际上还在,只是提前设置了标志位),对于工作队列的内核线程,需要进行一些特殊处理,毕竟这个内核线程关系到中断下半部的执行,这时候需要根据 workqueue 上的负载来确定是否需要唤醒其它的内核线程协助处理worker. 

做完上述的处理之后,需要对进程的 nvcsw 或者 nivcsw 进行更新,如果是自愿调度,则选中nvcsw,否则选中 nivcsw,多出的 'i' 表示 involuntarily.这里并没有操作，在后面选中了待运行进程且待运行进程不为 curr 的时候再对其进行加 1 操作。   

最后,再考虑一个问题:如果进程没有设置为非 TASK_RUNNING 状态的情况下,直接调用 schedule() 函数,会怎么样?当然,这和上面提到的关抢占然后调度的问题一样属于不正常的情况,对于这种情况的处理其实和检测到有未处理信号一样,调度器会照样选择下一个执行的进程,而且大概率会是当前进程.  

理论上来说,cfs 调度器上运行的就是 vruntime 最小的进程,如果是这样,下一个被选择的进程几乎一定是当前进程.但是具体的实现来理论还是有些差别,其中包括:
* 进程有一个最小执行时间的限制,可能当前进程的 vruntime 大于 leftmost,但是依旧在运行.
* 在周期性的调度检查中,并不是 curr->vruntime 大于 leftmost->vruntime 就立马调度,而是需要这两者的差值大于 curr 的 idle_time.可能当前进程的 vruntime 大于 leftmost,但是依旧在运行.
* 由于检查调度的粒度问题,进程已经超出理论应该运行的时间,但是没有出现检查是否需要调度的点.  

这三种情况都可能导致 leftmost->vruntime 小于 curr->vruntime,在这几种情况下执行调度,调度器就会选择到 leftmost,而不是继续执行当前进程了.  

关于这三种情况的描述,可以参考我的另一篇博客(TODO).  


### 选择下一个需要执行的进程
选择下一个需要执行进程的接口为 pick_next_task,这是内核核心调度部分的接口,针对所有的调度器类,而不单单是 cfs 调度相关,源码如下:

```c++
schedule->__schedule->pick_next_task

static inline struct task_struct *
pick_next_task(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
	const struct sched_class *class;
	struct task_struct *p;

	if (likely((prev->sched_class == &idle_sched_class ||
		    prev->sched_class == &fair_sched_class) &&
		   rq->nr_running == rq->cfs.h_nr_running)) {           ................................1

		p = fair_sched_class.pick_next_task(rq, prev, rf);      ................................2
		if (unlikely(p == RETRY_TASK))
			goto again;

		/* Assumes fair_sched_class->next == idle_sched_class */
		if (unlikely(!p))
			p = idle_sched_class.pick_next_task(rq, prev, rf);

		return p;
	}

again:
	for_each_class(class) {                                   .................................3
		p = class->pick_next_task(rq, prev, rf);
		if (p) {
			if (unlikely(p == RETRY_TASK))
				goto again;
			return p;
		}
	}
}
```
注1:在 pick_next_task 中,其实并不是想象中的直接按照调度器的优先级对所有调度器类进行遍历,而是假设下一个运行的进程属于 cfs 调度器类,毕竟,系统中绝大多数的进程都是由 cfs 调度器进行管理,这样做可以从整体上提高执行效率.   

判断一个下一个进程是不是属于 cfs 调度器的方式是当前进程是不是属于 cfs 调度器类或者 idle 调度器类管理的进程,同时满足 rq->nr_running == rq->cfs.h_nr_running, 其中 rq->nr_running 表示当前 runqueue 上所有进程的数量,包括组调度中的递归统计,而 rq 的 root cfs_rq 的 h_nr_running 表示 root cfs_rq 上所有进程的数量,如果两者相等,自然就没有其它调度器的进程就绪.  

注2:如果确定了下一个进程会在 cfs_rq 中选,就调用 cfs 调度器类的 pick_next_task 回调函数,这个我们在后面详细讨论.  

注3:如果确定下一个进程不在 cfs_rq 中选,就需要依据优先级对所有的调度器类进行遍历,找到一个进程之后就返回该进程,由此可以看出,在系统的调度行为中,不同的调度器类拥有绝对的优先级区分,高优先级的调度器类并不会与低优先级的调度器类共享 CPU,而是独占(会有一些特殊情况,在具体实现中,实时进程会好心地让出那么一点 CPU 时间给非实时进程,这个时间可配置).  

遍历调度器类的接口为 for_each_class,在多核架构中,最高优先级调度器类为 stop_sched_class,其次是 dl_sched_class,单核架构中,最高优先级调度器类为 dl_sched_class,接下来依次是 rt_sched_class->fair_sched_class->idle_sched_class. stop_sched_class 主要用来停止 CPU 的时候使用的,比如热插拔.其遍历过程也是通过调用对应调度器类的 pick_next_task 函数,如果该调度器有进程就绪就返回进程,否则返回 NULL,继续遍历下一个调度器类,至于其它调度器类的实现,暂不讨论.  

回过头来,我们再来看看 cfs 的 pick_next_task 实现:


```c++
schedule->__schedule->pick_next_task->pick_next_task_fair:

static struct task_struct *
pick_next_task_fair(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
	struct cfs_rq *cfs_rq = &rq->cfs;
	struct sched_entity *se;
	struct task_struct *p;
	int new_tasks;

again:

	// 组调度的情况,需要递归地选出进程
#ifdef CONFIG_FAIR_GROUP_SCHED                ................................1
	do {
		struct sched_entity *curr = cfs_rq->curr;

		if (curr) {
			if (curr->on_rq)
				update_curr(cfs_rq);
			else
				curr = NULL;
		}

		se = pick_next_entity(cfs_rq, curr);
		cfs_rq = group_cfs_rq(se);
	} while (cfs_rq);

	p = task_of(se);

	if (prev != p) {
		struct sched_entity *pse = &prev->se;

		while (!(cfs_rq = is_same_group(se, pse))) {
			int se_depth = se->depth;
			int pse_depth = pse->depth;

			if (se_depth <= pse_depth) {
				put_prev_entity(cfs_rq_of(pse), pse);
				pse = parent_entity(pse);
			}
			if (se_depth >= pse_depth) {
				set_next_entity(cfs_rq_of(se), se);
				se = parent_entity(se);
			}
		}

		put_prev_entity(cfs_rq, pse);
		set_next_entity(cfs_rq, se);
	}

	return p;
simple:
#endif

	...
	/* 不支持组调度的情况*/
	...

idle:
	new_tasks = idle_balance(rq, rf);      .......................................2
	
	if (new_tasks < 0)
		return RETRY_TASK;

	if (new_tasks > 0)
		goto again;

	return NULL;
}
```

对于 pick_next_task_fair 来说,根据是否支持组调度区分处理,不支持组调度的处理就是支持组调度处理方式的子集,因此不贴出这部分代码,只讨论支持组调度的情况:

在函数执行之初,根据 curr->on_rq 的值对 curr 进行赋值,__schedule() 的调用分为自愿和非自愿调度,自愿调度的情况下 curr->on_rq 在这之前已经被设置为 0,而非自愿调度 curr->on_rq 依旧是 1. 

接着调用实际的选择下一个实体的函数:pick_next_entity,后面讨论.  

通过 cfs_rq = group_cfs_rq(se) 的返回判断当前的调度实体是 group se 还是进程 se,如果是 group se,其 my_rq 返回值为该 group se 的自有调度队列,下一步就需要递归进入到该自有队列上,直到选出一个合适的 task_se,再通过 task_se 获取到对应的进程,这就选出了一个合适的待运行进程.   

一方面是选出合适的进程,另一方面则是处理当前进程,如果是自愿调度,当前进程已经在 __schedule 中处理得差不多了,而如果是抢占调度,就需要设置当前进程 se 的状态,比如更新时间参数,同时将其放回到红黑树中.而对于组调度而言,事情总是要稍微复杂一些,同样地需要递归向上地进行处理,如果当前进程和下一个进程不在同一个组内,就需要递归向上,将当前进程的 se->parent 从同级的 cfs_rq->curr 摘下来,对应的接口为 put_prev_entity.  

在递归处理进程 se 的同时,也需要设置下一个执行的实体,做调度的前期工作,对应的接口为 set_next_entity,该函数完成以下的任务:
* 如果 se->on_rq 为 1,需要调用 __dequeue_entity 将该 se 从红黑树中摘下来,同时将 cfs_curr 设置为当前 se,
* 更新一些统计信息,比如从就绪到实际执行的时间,负载信息等. 
* 将 se->prev_sum_exec_runtime 赋值为 se->sum_exec_runtime,当进程在运行时,这两者的差值反映了进程本次从进入运行态开始所运行的时间.   

注2:如果选中了合适的进程,就返回对应的进程,以执行下一步的进程切换工作,如果没有合适的进程,将会触发负载均衡,把其它 CPU 上的进程移到当前 CPU,再重新执行上面的过程,如果其它 CPU 上也没啥进程需要迁移,就执行 idle 进程.  

从上面的解析可以看出,pick_next_task 并不仅仅是选出下一个应该执行的进程,同时还处理了之前的进程实体(以及父级se),调用 put_prev_task,以及设置下一个进程在 cfs_rq 中的状态 set_next_entity.   


接下来就看看选择合适进程的具体实现，对应的函数为：

```c++
schedule->__schedule->pick_next_task->pick_next_task_fair->pick_next_entity:

static struct sched_entity *
pick_next_entity(struct cfs_rq *cfs_rq, struct sched_entity *curr)
{
	struct sched_entity *left = __pick_first_entity(cfs_rq);
	struct sched_entity *se;

	// 如果红黑树上 vruntime 最小的进程不存在或者其 vruntime 值大于当前运行的进程，则选择当前进程 
	if (!left || (curr && entity_before(curr, left)))
		left = curr;

	se = left; 

	// 如果被选中的进程被设置为 skip 进程，表明系统并不想它在下次运行，就需要重新选择进程实体
	if (cfs_rq->skip == se) {
		struct sched_entity *second;

		// 如果原本应该下次执行的是 curr，就重新从红黑树上取一个。
		if (se == curr) {
			second = __pick_first_entity(cfs_rq);
		} 
		//  否则，就从红黑树上取出 vruntime 第二小的，再跟当前的 curr 比较。  
		else {
			second = __pick_next_entity(se);
			if (!second || (curr && entity_before(curr, second)))
				second = curr;
		}

		// 见下文
		if (second && wakeup_preempt_entity(second, left) < 1)      
			se = second;
	}

	if (cfs_rq->last && wakeup_preempt_entity(cfs_rq->last, left) < 1)  
		se = cfs_rq->last;

	if (cfs_rq->next && wakeup_preempt_entity(cfs_rq->next, left) < 1)  
		se = cfs_rq->next;

	// 见下文
	clear_buddies(cfs_rq, se);

	return se;
}

```

对于如何选择下一个运行的实体，规则倒是很简单：就是从红黑树上找到 vruntime 值最小的进程实体，实际上并没有操作红黑树，而是使用的缓存节点，cfs_rq 在每次平衡红黑树的时候都会把最左边的节点缓存起来，以加速当前函数的执行。  

找到最小的实体之后，如果是抢占(非自愿)调度，当前进程还在 cfs_rq 上，会和当前运行的进程做对比，如果 vruntime 小于当前进程，就设置为 leftmost，否则依旧选择当前进程作为下次运行的进程，如果是自愿调度，通常当前进程就已经出队了，就只能从红黑树上面选了。    

在调用 pick_next_entity 选择下一个进程时，存在一些特殊的处理和一些人为的调整，遵循下面的规则：
* 保持进程/进程组之间的公平是第一要素，但是也并不是绝对公平，在公平允许的范围内可以人为地做一些调整。
* 如果存在 cfs_rq->next，说明内核特意地想让某个进程在下一次调度中运行，这个 next 可以获得一些优先权。 
* 如果存在 cfs_rq->last，该 last 实体是上次在 CPU 上运行过的，对于该进程的数据很大概率还存在于cache中，执行它可以获得更高的效率，它可以获得一些优先权。 
* 如果存在其他可选的实体，不选择 cfs_rq->skip 实体作为下次运行的实体。

其实从源码可以看出，上面四个规则都不是绝对的，当选出来的进程被设置为 skip 时，会尝试选择第二个可运行的进程，再对第二个进程和 curr 进行对比，但是如果实在找不到第二个合适的(curr == NULL 且没有其它合适进程)，还是会运行这个被 skip 标记的进程。  

next 的优先级要比 last 大，但是它们都需要通过 wakeup_preempt_entity 函数来判断运行对应的进程会不会造成很大的不公平，wakeup_preempt_entity 函数接收两个参数，curr 和 se，该函数的整体含义为：判断 se 是否应该抢占 curr，当然，抢占这个说法其实是不大合理的，这两个实体都没有运行，只是待运行，这个函数名用在这里是不合时宜的(包括wakeup)，大概是因为这个函数原本是用来判断唤醒进程是否可以抢占当前进程，只是可以复用在判断两个进程谁更适合运行吧。 

se "抢占" curr 是有条件的，第一个自然是 se->vruntime 要小于 curr->vruntime，同时也并不是小于就行，而且这个差值要达到一定的阈值，这个阈值是动态计算的，由 se 的权重决定。  

wakeup_preempt_entity 中调用了我们熟悉的 calc_delta_fair 函数，这个函数在计算 vruntime 的时候常用，这里不再赘述，它的大致实现原理就是：第一个参数为基准值，这里为 sysctl_sched_wakeup_granularity(1ms，可调整)，第二个参数为目标 se，以优先级 120 为基准，se 的优先级每高出 1，就对基准值乘以 1.25，每低出 1，就对基准值除以 1.25，再返回基准值。

返回的基准值作为阈值，和 (curr - se) 进行对比，来决定 se 是否可以抢占 curr。  

wakeup_preempt_entity 这个函数会在判断进程抢占中经常使用，这种计算方法也就体现出进程优先级的差别，高优先级的进程也就能更快地执行。 


### 执行进程切换
经过千辛万苦，终于选到了合适的进程，在选取了合适的待运行进程之后,就进入下一个环节:进程的切换.  

如果是抢占调度，就需要先清除抢占标志。  

然后，判断选出来的下一个进程是否是当前运行的进程，经过上面的源码分析，其实资源调度和抢占调度两种方式选到的待运行进程都可能是当前进程，这种情况下就不需要做什么处理，直接清理调度设置准备退出。  

大部分情况下待运行进程都不会是 curr，这时候就需要进入到真正的切换流程，在切换之前，执行一些必要的计数统计和更新：

```c++
rq->nr_switches++;
rq->curr = next;
++*switch_count;   // 这个变量是 curr->nivcsw 或者 curr->nvcsw
```

然后执行切换函数：context_switch。 

```c++
schedule->__schedule->context_switch:

static __always_inline struct rq *
context_switch(struct rq *rq, struct task_struct *prev,
	       struct task_struct *next, struct rq_flags *rf)
{
	struct mm_struct *mm, *oldmm;

	// 切换的准备工作，包括更新统计信息、设置 next->on_cpu 为 1 等。 
	prepare_task_switch(rq, prev, next);
	
	// 获取即将切换到的进程的 mm。
	mm = next->mm;
	// 当前的 active_mm
	oldmm = prev->active_mm;
	
	// mm = NULL 表示下一个进程还是一个内核线程
	if (!mm) {
		// 如果是内核线程，依旧不需要切换 mm，依旧保存 active_mm
		next->active_mm = oldmm;
		// 相当于添加引用计数：atomic_inc(&mm->mm_count);
		mmgrab(oldmm);
		enter_lazy_tlb(oldmm, next);
	} else
		switch_mm_irqs_off(oldmm, mm, next);

	if (!prev->mm) {
		prev->active_mm = NULL;
		rq->prev_mm = oldmm;
	}

	...

	rq_unpin_lock(rq, rf);
	spin_release(&rq->lock.dep_map, 1, _THIS_IP_);
	 
	switch_to(prev, next, prev);
	barrier();

	return finish_task_switch(prev);
}
```

从硬件上来说，CPU 为了加速数据的访问过程，大量地使用了缓存技术，包括指令、数据和 TLB 缓存，事实证明，缓存的使用给系统运行效率带来了巨大的提升。在多核架构中，缓存是 percpu 的，通常只有 L3 缓存的 global 的，因此，对于进程的切换而言，通常操作的都是本地 CPU 的缓存。    

在 context_switch 中，一个比较重要的部分是用户空间内存映射的处理，即 mm 结构，整个用户空间的内存布局都是由一个 struct mm 的结构保存的，对于内核线程而言，并没有所谓的用户空间的概念，其 mm 为 NULL。  

内核是统一的，并不像用户空间一样每个进程享有独立的空间，因此，从缓存的角度来看，对于内核部分和用户空间部分的处理是完全不一样的。对于指令和数据 cache 而言，会随着程序的执行逐渐被替换，这对于用户空间和内核空间都是一样的，只是可能内核空间中的某些 cache 依旧可以利用在下一个进程。  

重点在于 TLB 的缓存处理，TLB 中缓存了页表对应的虚拟地址到物理地址的映射，有了这一层缓存，对某片内存的重复访问只需要从缓存中取，而不需要重新执行翻译过程，当执行进程切换时，尽管每个进程虚拟空间都是一致的，但是其对应的物理地址通常是不相等的，因此需要将上个进程的 TLB 缓存清除，不然会影响下一个进程的执行。  

但是实际情况却有一些优化空间，比如对于用户进程之间的互相切换，其用户空间的 TLB 缓存自然是要刷新，但是内核空间可以保留，因为内核中是共用的。同时，linux 中的内核线程和用户空间没有关系，假设存在这样的情况：用户进程 A 切换为内核线程 B，内核线程 B 运行完之后又切换回 A，这时候从 A 切换到 B 的时候如果清除了 TLB 缓存，在 B 切换回 A 的时候，TLB 有需要重新填充，实际上这种情况可以在切换到内核线程时保留用户空间的 TLB 缓存，如果又切换回 A 的时候就正好可以直接使用，提升了效率。  

如果此时从 B 切换到其它内核线程 C，TLB 缓存依旧可以保留，直到切换到下一个用户进程，如果这个进程不是原本的 A，这时候才会把 TLB 清除，这就是我们经常听到的 "惰性 TLB"，这种 lazy operation 可以在很多地方可以看到，比如 fork 的执行，动态库的绑定，其核心思想就是直到资源在真正需要的时候才进行操作。

上面的源代码就是基于上述的逻辑，获取 next->mm ，这是待运行进程的 mm，同时获取 oldmm = prev->active_mm，如果当前进程是用户进程，oldmm 等于 NULL，因为切换到其它用户进程时 mm 是肯定需要替换的，而如果当前进程是内核线程，oldmm 就是上个进程保留的 mm，当然，上个进程也可能是内核线程，这里保存的就是上一个用户进程的 mm。 

判断如果待运行进程的 mm 为空，即为内核线程，那么就不需要切换 mm 结构，相对应的 TLB 也不需要刷新，而如果待运行进程是用户进程，分两种情况：第一个是缓存的 mm 正好是下一个待运行进程，也就不需要做什么事，另一种情况就是当前进程不是缓存的 mm，那么就需要替换 mm，然后刷新 TLB，同时只需要刷新用户空间的 TLB，而不需要刷新内核 TLB。  


#### switch_to
在处理完上下文环境之后，就来到了真正的进程切换操作了，具体操作自然是和硬件强相关的，被定义在 arch/ 下，同时操作的代码都是通过汇编实现的。  

对于进程的实际切换，有两个点需要弄清楚：

第一点就是，switch_to 是进程的一个断点，也就是在 switch_to 中，会完成进程的切换，在 switch_to 之后的代码，实际上是在进程切换回来之后执行的。比如，从上面的代码来看，switch_to 后调用了 finish_task_switch，实际上，进程 A 在调用 switch_to 中切换到了进程 B，而紧接着执行的是 B 的 finish_task_switch，因此，在 B 中的 finish_task_switch 中做的一些调用清理工作，其实是针对进程 A 的。  

第二点也就是基于第一点的延伸，switch_to 的调用为;switch_to(prev, next, prev),函数需要传入三个参数，按理说，只需要两个参数，一个是当前进程，一个是 next，也就是待运行进程，为什么还需要第三个参数？   

switch_to 中完成了进程的切换，switch_to 之后的代码实际上是进程切换回来执行的，那么，我们何从知道当前进程是从哪个进程切换过来的呢?所以，多出来的一个参数就是上次运行的进程，实际上是 last，比如由 A 切换到 B，对于 B 而言，多出来的一个参数保存的就是 A 的 task_struct.  

在 arm 的实现中，switch_to 将会调用 __swtich_to：

```c++
#define switch_to(prev,next,last)					\
do {									\
	__complete_pending_tlbi();					\
	last = __switch_to(prev,task_thread_info(prev), task_thread_info(next));	\
} while (0)
```
通过 switch_to 的定义更能体现切换的过程，切换之后将会返回上次运行的进程。而 __switch_to 函数则被定义在 arch/arm/kernel/entry-armv.S 中。这部分汇编代码的实现为：
* 将当前进程大部分通用寄存器保存到栈上，需要注意的是，这里并不是压栈，因为每个进程的内核栈的底部保存的是当前进程的 thread_info,在 arm 中，保存寄存器的地址为：thread_info->cpu_context,保存的通用寄存器的值为 r4 ~ r14，其中 lr 是 __switch_to 函数的返回值。
* 保存或者替换其它的寄存器值，比如线程本地存储 tls、浮点寄存器或者其它，这一部分是硬件强相关的，需要看硬件是否提供对应的功能。 
* r0 寄存器保持原样，传入的 r0 是第一个参数，表示当前进程，切换到另一个进程 B 之后，其返回值就是当前进程 A，也就是 B 可以知道是 A 切换到它的。   
* 将 next 进程对应内核栈上保存的寄存器值恢复到寄存器中，也就完成了切换，r4~r13 是原样恢复，而 pc 指针则使用保存的 lr 寄存器，也就是新进程会从 __switch_to 函数返回处开始执行。 


## 收尾工作
经过 switch_to 函数，进程已经从 A 切换到 B 了，prev 的值也是 A 进程，B 进程从 switch_to 返回处开始执行：

```c++
	barrier();
	return finish_task_switch(prev);
```

barrier 是一个双向的内存屏障，显示地隔开前后的代码，避免 CPU 乱序执行，这里也很好理解，switch_to 前后都不是一个进程了，自然不能再乱序了。  

对于 finish_task_switch，主要做一些收尾的工作，比如当之前的 mm 不再被引用时，将其释放掉，如果上一个进程的状态为 DEAD，需要释放掉上一个进程的相关资源，


