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

做完上述的处理之后,需要对进程的 nvcsw 或者 nivcsw 进行更新,如果是自愿调度,nvcsw 加1,否则 nivcsw,多出的 'i' 表示 involuntarily.  

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

### 执行进程切换
在选取了合适的待运行进程之后,就进入下一个环节:进程的切换.  




