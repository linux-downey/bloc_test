# linux调度子系统15 - 负载均衡源码实现



## load_balance

load_balance 是完成负载均衡工作的主要操作函数，这个函数的原型为:

```c++
static int load_balance(int this_cpu， struct rq *this_rq，
			struct sched_domain *sd， enum cpu_idle_type idle，
			int *continue_balancing);
```

该函数接受五个参数，执行负载均衡的 CPU 和 rq，这个 CPU 也是相对空闲，需要将进程移进来的 CPU.  

尽管前面分析了负载均衡的执行背景，这里有必要再重复一下:load_balance 是在一个 for_each_domain 的循环中被调用的，也就是说，在一次负载均衡中 load_balance 可能不只是调用一次，随着每一次的调用，sd 参数将会更新为 sd->parent，也就是从当前 domain 往上遍历，而 continue_balancing 这个参数其实是将是否需要继续执行 balance 的信息返回给调用者.  

同时，就目前我所理解的内核代码而言，当需要执行负载均衡时，具体的进程移动操作由空闲 CPU 完成，而不会由其它 CPU 代劳，比如 CPU1 检测到 CPU3 应该向空闲的 CPU2 迁移进程，CPU1 并不会去操作，而是等 CPU2 自己执行 idle_balance 或者周期性的 balance 来移动进程，这应该是出于 rq lock 竞争和 cache 命中的考虑.  

而 idle 这个标志位，代表当前执行 load_balance 的 CPU 的状态，主要是三个:CPU_NEWLY_IDLE 表示 CPU 刚准备进入到 idle 状态，这是在 idle_balance 时传入的标志位，CPU_IDLE 和 CPU_NOT_IDLE 则表示 CPU 的空闲与否，如果是 CPU_IDLE，也可能是 CPU 早就处于空闲状态下了，这两个标志位在周期性的负载均衡中使用，通过 idle_cpu(cpu) 可以判断 CPU 是否空闲，再传入到 load_balance 函数中.不同的标志位代表不同的负载均衡紧迫性，因此处理上也不大一样.    

load_balance 是一个很长的函数，按照一贯的风格，并不会逐行地分析源码，而是针对负载均衡的操作以及背后的原理进行逐步的分解，然后贴出关键代码用以佐证，毕竟内核代码是一直在变的，而不变的只有其背后的逻辑.  

load_balance 函数的工作主要分成以下几个部分:



* 再次确定是否有 balance 的需求
* 找到最忙的 CPU
* 将进程从最忙的 CPU 上 detach
* 将 detach 的进程 attach 到当前 CPU 上
* active balance

下面根据源码一一对这些部分进行分析.  



### struct lb_env

load balance 并不是一项简单的工作，可能需要跨 domain 进行处理，通常也包含多个 CPU 的相关操作，因此，内核中使用 struct lb_env 这个结构体来记录一些中间状态以及参数. 

其中最主要的几个成员变量为:

```c++
struct lb_env {
	struct sched_domain	*sd;   // 负载均衡当前所属的 domain

	struct rq		*src_rq;   // 进程迁移的源 rq，也就是相对繁忙的 rq
	int			src_cpu;       // 进程迁移的源 CPU

	int			dst_cpu;       // 进程迁移的目标 CPU，也就是相对空闲的 cpu
	struct rq		*dst_rq;   // 进程迁移的目标 rq

	struct cpumask		*dst_grpmask;  // 目标 sched_group 的 CPU mask 掩码位. 
	int			new_dst_cpu;     
	enum cpu_idle_type	idle;   // 执行进程迁移的 CPU 所处的状态，参考上文.
	long			imbalance;  // 该变量记录不平衡的程度，见下文分析

	struct cpumask		*cpus;  // CPU 掩码

	unsigned int		loop;
	unsigned int		loop_break;
	unsigned int		loop_max;   // 这三个变量用来控制扫描的时间，见下文分析.   

	// 这是一个链表头，被迁移的进程会挂到当前链表上，在 dst CPU attach 的时候也是从这个链表上取.
	struct list_head	tasks;   
};
```


### 确认 load_balance 的需求

对于 idle_balance 而言，负载均衡的需求自然是明确的，而对于周期性执行的负载均衡却不一样，需要先确定是否真的需要执行负载均衡，这个判断由 load_balance->should_we_balance 函数完成:

```c++
static int should_we_balance(struct lb_env *env)
{
	struct sched_group *sg = env->sd->groups;
	int cpu， balance_cpu = -1;
	if (!cpumask_test_cpu(env->dst_cpu， env->cpus))   ......................................1
		return 0;
	
	if (env->idle == CPU_NEWLY_IDLE)
		return 1;

	for_each_cpu_and(cpu， group_balance_mask(sg)， env->cpus) {  
		if (!idle_cpu(cpu))
			continue;

		balance_cpu = cpu;
		break;
	}

	if (balance_cpu == -1)
		balance_cpu = group_balance_cpu(sg);

	return balance_cpu == env->dst_cpu;     ................................2
}
```

注1:env->dst_cpu 是相对空闲的当前 CPU，而 cpus 是一个 percpu 的全局变量，表示可用于负载均衡的 CPU 掩码，在这里重复进行检查的意义在于:确保 balance 的 CPU 环境是固定的，因为当前程序可能与系统的 CPU hotplug 程序并发执行，会造成一些问题，且在周期性的负载均衡中，当前程序处于 softirq 环境，优先级高于进程，如果这期间确实发生了 CPU hotplug 事件，返回0，表示本次 load_balance 函数执行终止.  

注2:从当前 domain 的当前 group 中，尝试找到第一个处于执行 idle 进程的 CPU，如果没有找到，就选择该组内 capacity 最大的 CPU，如果这个 CPU 不是 dst_cpu，返回0，同样表示本次 load_balance 函数执行终止.

因此，从这里可以看出，每一次负载均衡的 dst CPU 都是固定的(当前 CPU)，如果检查到应该作为 dst 的 CPU 不是当前 CPU，就不需要执行 load_balance.  

同时，如果 should_we_balance 返回 0， load_balance 的 continue_balancing 参数也被设置为 0，表示本次整个的负载均衡操作都不需要再往下进行了.  



### 找到最忙的 CPU

在为即将加入就绪队列的进程选择 CPU 时，使用的是 find_idlest_* 相关的接口，当前操作是需要找到最忙的 CPU，相对应的，对应的接口为:

```c++
static int load_balance(int this_cpu， struct rq *this_rq，
			struct sched_domain *sd， enum cpu_idle_type idle，
			int *continue_balancing)
{
	struct sched_group *group;
	struct rq *busiest;
	group = find_busiest_group(&env);

	busiest = find_busiest_queue(&env， group);
	...
}
```

find_busiest_group 将会遍历当前 domain 的 groups 链表上挂着的所有 group，通过计算 group 的负载来确定哪个 group 是最繁忙的，这个负载是在进程调度的过程中统计的. 同时，在 find_busiest_group 中有一个非常重要的操作，就是调用 calculate_imbalance 计算 group 中的不平衡程度，这是一个 long 型变量，该值越大，表示不平衡的程度越大.  

同样的，find_busiest_queue 接口会遍历当前 group 下的 CPU，找到最繁忙的 CPU 以确定对应的 runqueue.  

关于负载统计的细节这里不作讨论.   




### 进程的 detach

find_busiest_queue 确定了负载迁移的源 CPU，下一步的工作就是从源 CPU 上取出进程到 dst CPU 上.

```c++
static int load_balance(int this_cpu， struct rq *this_rq，
			struct sched_domain *sd， enum cpu_idle_type idle，
			int *continue_balancing)
{
	...
	env.src_cpu = busiest->cpu;
	env.src_rq = busiest;

	ld_moved = 0;
	if (busiest->nr_running > 1) {

		env.flags |= LBF_ALL_PINNED;
		env.loop_max  = min(sysctl_sched_nr_migrate， busiest->nr_running);

more_balance:
		rq_lock_irqsave(busiest， &rf);   ..................................1
		update_rq_clock(busiest);

		cur_ld_moved = detach_tasks(&env);  ...............................2
		rq_unlock(busiest， &rf);

		if (cur_ld_moved) {
			attach_tasks(&env);       ......................................3
			ld_moved += cur_ld_moved;
		}

		local_irq_restore(rf.flags);

		if (env.flags & LBF_NEED_BREAK) {
			env.flags &= ~LBF_NEED_BREAK;
			goto more_balance;
		}
}
```

如果要从 busiest CPU 上迁移进程，该 rq 就绪队列上的进程数量自然要大于 1，如果小于等于 1，该 group 可能同样存在不平衡的状态，这种情况需要交到 active balance 处理，这部分我们在下面讨论.  

不难想到，从源 CPU 上 detach 进程的操作需要遍历源 CPU 的 rq，这种操作其它 CPU rq 的行为自然是需要上锁的，同时还需要关中断，避免与中断的竞争，但是，在内核中关中断并不是推荐的做法，这直接影响到当前 CPU 执行程序的实时性，因此，即使是在无奈的情况下确实是需要关中断做一些事情，也要尽量地保证关中断的时间不能太长，这是内核中的基本原则.  

因此，内核使用了 loop/loop_max/loop_break 来管理遍历时间，从命名可以看出，loop_max 表示当前遍历一次的最长时间限制，loop_break 表示遍历超过一定的时间，休息一下，先打开中断避免对系统实时性造成太大影响，当待处理的中断返回时再继续处理，loop 变量则是用来记录遍历已经花费的时间.  

进程的 detach 工作主要由 detach_tasks 函数完成，这个函数将扫描源 CPU 的 rq，然后将合适的进程从源 CPU rq 上 dequeue，并将这些进程链接在 env->tasks 上，返回 detach 的进程数量，保存在 cur_ld_moved 中.  

detach_tasks 的实现如下:

```c++
static int detach_tasks(struct lb_env *env)
{
	struct list_head *tasks = &env->src_rq->cfs_tasks;

	if (env->imbalance <= 0)          ...............................1
		return 0;
	
	while (!list_empty(tasks)) {

		...

		p = list_first_entry(tasks， struct task_struct， se.group_node);

		env->loop++;
		if (env->loop > env->loop_max)
			break;

		if (env->loop > env->loop_break) {         .....................2
			env->loop_break += sched_nr_migrate_break;
			env->flags |= LBF_NEED_BREAK;
			break;
		}                                                      

		if (!can_migrate_task(p， env))             ................3
			goto next;

		load = task_h_load(p);

		if (sched_feat(LB_MIN) && load < 16 && !env->sd->nr_balance_failed)
			goto next;

		if ((load / 2) > env->imbalance)
			goto next;

		detach_task(p， env);     
		list_add(&p->se.group_node， &env->tasks);   ...............4

		detached++;
		env->imbalance -= load;

#ifdef CONFIG_PREEMPT
		if (env->idle == CPU_NEWLY_IDLE)
			break;
#endif

		if (env->imbalance <= 0)                   .................5
			break;

		continue;
next:
		list_move_tail(&p->se.group_node， tasks);  
	}

	return detached;
}
```

注1:env->imbalance 表示 group 中的不平衡程度，同时，每个进程都存在一个"对 CPU 的负载贡献"的概念，CPU 的负载大小表明了 CPU 的繁忙程度，是由 CPU 就绪队列上的进程决定的，当一个进程执行时，自然是对 CPU 的负载产生了贡献，同时即使进程只是放置在 CPU 的就绪队列上，也是一样.比如一个 CPU 的 rq 上存在 3 个就绪进程，而另一个 CPU 的 rq 上存在 8 个就绪进程，谁的负载更大一目了然.   

因此，一旦一个进程被添加到就绪队列，它就对 CPU 产生负载贡献并记录下来，如果一个进程负载贡献大，那么很明显这个进程对 CPU 的需求就大，以进程的负载贡献来判定各 CPU 的负载比单纯地使用进程个数来判定 CPU 上的负载自然是要更科学.  

每 detach 一个进程，env->imbalance 将会减去该进程的 load(负载贡献)，直到 env->imbalance 接近 0 的时候，表明此时两个 CPU 上的负载已经接近均衡，因此在 detach_tasks 中可以看到不少的 env->imbalance 的身影.  

注2:CPU 上有效进程被链接在 rq->cfs_tasks 上，对进程的遍历也就是对这个链表的遍历，遍历在一个 while 循环中进行，而且这个遍历是循环进行的，每遍历完一个节点都会将该节点放到链表末尾，也就是如果不使用 break，这个链表会一直循环遍历下去.每遍历一次，loop 都会自增，这个变量用于记录遍历次数，正如上文所说，当 loop 超过了 loop_max，表明关中断的时间已经足够长，遍历到此为止，loop break 和 loop_max 的区别在于，当 loop 达到 loop_break 的阈值，设置 LBF_NEED_BREAK 的标志位，表示需要休息一下，先开中断.  

注3:在负载均衡中，并不是所有的进程都可以进行迁移，内核通过 can_migrate_task 函数来判断进程是否能迁移，见下文.

注4：如果判断目标进程可以迁移，就取出该进程的 load，这个 load 是进程对于 CPU 的负载贡献，在过去的时间中，该进程表现得对进程的需求越大，load 也就越高。当设置了 LB_MIN 调度特性时，该特性会针对那些 load 值较小的进程迁移，这些进程对 CPU 的需求并不旺盛，因此迁移的必要性并不会特别大，这只是个软限制，如果没有其它进程可迁移，那么这个限制也会被打破(nr_balance_failed 表示进程迁移失败的次数，见下文 can_migrate_task 的分析)。  

同时，如果 CPU 之间的不平衡并不是很大，当前进程的 load 值除以 2 之后仍旧大于不平衡系数，也不需要迁移。  

如果进程满足了迁移的条件，就会调用 detach_task 将进程从就绪队列中移除，这个函数可以参考[se 的 enqueue 和 dequeue](https://zhuanlan.zhihu.com/p/363791956)，然后将移除就绪队列中的进程记录在 env->tasks 链表上。  

注5：当 CPU 之间的不平衡已经被 fix，也就是 imbalance 值小于等于 0，也就不再不需要执行本次的进程迁移，心满意足地退出，然后执行下一个 domain 内的扫描或者退出。  

从最后的 list_move_tail(&p->se.group_node， tasks); 可以看出，这个遍历 rq 的操作是重复循环的。  

分析完 detach_tasks，了解了进程迁移的是如何 detach 进程之后，把视角再拉回到 can_migrate_task 函数，看看调度器如何判断进程是否合适迁移。can_migrate_task 对应的源码为：

```c++
load_balance->detach_tasks->can_migrate_task:

static int can_migrate_task(struct task_struct *p， struct lb_env *env)
{
	if (throttled_lb_pair(task_group(p)， env->src_cpu， env->dst_cpu))   ..........6
		return 0;
	
	if (!cpumask_test_cpu(env->dst_cpu， &p->cpus_allowed)) {           ...........7
		int cpu;

		schedstat_inc(p->se.statistics.nr_failed_migrations_affine);

		env->flags |= LBF_SOME_PINNED;

		if (env->idle == CPU_NEWLY_IDLE || (env->flags & LBF_DST_PINNED))
			return 0;

		for_each_cpu_and(cpu， env->dst_grpmask， env->cpus) {
			if (cpumask_test_cpu(cpu， &p->cpus_allowed)) {
				env->flags |= LBF_DST_PINNED;
				env->new_dst_cpu = cpu;
				break;
			}
		}

		return 0;
	}

	if (task_running(env->src_rq， p)) {                                   ..........8
		schedstat_inc(p->se.statistics.nr_failed_migrations_running);
		return 0;
	}

	tsk_cache_hot = migrate_degrades_locality(p， env);
	if (tsk_cache_hot == -1)
		tsk_cache_hot = task_hot(p， env);

	if (tsk_cache_hot <= 0 ||
	    env->sd->nr_balance_failed > env->sd->cache_nice_tries) {          ..........9
		if (tsk_cache_hot == 1) {
			schedstat_inc(env->sd->lb_hot_gained[env->idle]);
			schedstat_inc(p->se.statistics.nr_forced_migrations);
		}
		return 1;
	}

}
```

注6：如果进程即将因为 throttled 而被移出就绪队列，这种情况下就不需要再将该进程进行迁移了。  

注7：判断当前进程的 cpus_allowed 字段是否设置了 dst_cpu，如果进程不允许被运行在 dst_cpu 上，自然是不能进行迁移的。  

但是事情并没有完，调度器实际上会根据 domain 从下至上遍历 domain 进行进程的迁移工作，如果因为src CPU 上的进程因为 cpus_allowed 的设置而不对进程做任何操作，同时如果当前 CPU 是整个系统中最繁忙的 CPU 的话，那么会出现每次遍历都会选择该 CPU 作为 src CPU，因为它就是最忙的，而且也没有迁移进程出去，这个 CPU 就在遍历时被重复选中，但是却无法真正地完成迁移的工作。  

因此，为了避免出现这种情况，调度器会将这些无法迁移的进程先迁移同组的其他可迁移的 CPU 上，在这里的操作就是设置 env->new_dst_cpu 为同组的 CPU，<span id="jump">这种操作</span>其实是不符合迁移进程由本 CPU 完成的基本设定的，让当前运行的 CPU 处理另外两个 CPU 之间的进程迁移。 


注8：如果进程正在运行，自然是不方便做迁移的，判断的方式就是通过 p->on_cpu 标志位。  

注9：涉及到 cache 的问题，如果 cache 是热的，其实是不建议迁移的，但是当我们把视角放高一些再看，detach_tasks 中实际上是循环遍历整个进程链表，而且是重复地来回遍历，当遇到上述的几种情况而导致进程迁移失败时，env->sd->nr_balance_failed 就会自增，从 if ( env->sd->nr_balance_failed > env->sd->cache_nice_tries) 可以看出，当实在找不到合适的进程来迁移时，即使是 cache hot 的进程也只能妥协，将该进程迁移出去，毕竟进程迁移的目的是提高程序执行效率，而 cache hot 的考虑也是出于执行效率，两者冲突的时候自然是取效率更好的做法。从这里可以看出 cache hot 并不会禁止进程进程迁移，这只是一个软限制。  




## 进程的 attach

detach_tasks 函数将 detach 的进程挂在 env->tasks 链表上，并返回 detach 的进程数量，如果 detach 的数量不为 0，就需要将挂在 env->tasks 上的进程 attach 到 dst CPU 上，通常也是当前 CPU。 

attach 的操作由 attach_tasks 函数完成，这个函数并没有什么特殊的，就是将 env->tasks 上的进程一个个地取下来，然后调用 attach_task 函数，这个函数会将进程添加到就绪队列中，同时也会检查是否需要抢占 dst CPU 的 curr 进程。   

在进程 attach 完成之后，还有两部分需要处理：

* 判断 env.flags & LBF_NEED_BREAK 是否为真：detach_tasks 的执行处于关中断的环境，如果整个 task 的扫描因为达到 loop_break 的阈值而退出，会设置 LBF_NEED_BREAK 标志位，在本次 attach 完成之后，会再次调用 detach_tasks 继续进行扫描迁移工作。  
* 在 can_migrate_task 中如果因为 cpus_allowed 的原因导致无法迁移，但是出于不能让一个 CPU 被重复选中的考虑，会将进程迁移到组内其它 CPU 上，具体的迁移操作就在这里设置，具体的源码如下：

```c++
if ((env.flags & LBF_DST_PINNED) && env.imbalance > 0) {

	cpumask_clear_cpu(env.dst_cpu， env.cpus);

	env.dst_rq	 = cpu_rq(env.new_dst_cpu);
	env.dst_cpu	 = env.new_dst_cpu;
	env.flags	&= ~LBF_DST_PINNED;
	env.loop	 = 0;
	env.loop_break	 = sched_nr_migrate_break;

	goto more_balance;
}
	
```

迁移的前提是 env.imbalance 依旧大于 0，这时候会将 env.dst_cpu 设置为组内选定的其他 CPU，然后跳转到 more_balance 重新执行 detach_tasks 和 attach 函数，将 src_cpu 上的进程迁移到 dst_cpu 上。   

在内核中，这种操作不会很频繁，同时这种操作可能造成新的不平衡，需要在下次修正，不过总体来说，这种操作造成的问题要小于所带来的收益。  




## active balance

当调度器选出来的 busiest rq 上的进程不大于 1 ，也就是只有一个正在运行的进程(或者 detach 过程实在没有找到合适进程)，按理说这种情况并不是一种不平衡，因为系统的负载已经非常轻了，没有额外的进程可以进行迁移。  

上面的假设是基于 SMP 同构系统下，也就是当前 group 中的 CPU 是同质的，实际上，在一些特殊情况下，因为某些不得已的原因，进程运行在 misfit 的 CPU 上，这种情况同样会被视为一种不平衡的情况，需要将正在运行的进程移动到合适的 CPU 上。  

比如 arm 的大小核架构中，可能出现进程正运行在小核上，而大核处于 idle 状态，大核的程序运行效率是大于小核的，因此，针对这种情况，需要将小核中的进程停下来，然后将其迁移到大核中，以提高进程运行的效率。  

这就是 active balance 的主要设计思想，将正在运行的进程迁移到更合适的 CPU 上。  

CPU 在初始化时存在一个 capacity 的概念，从字面来说，capacity 表示一个 CPU 的能力，这个 capacity 的基准值由硬件提供，通过设备树传递，在内核初始化的时候设置。  

再来看看源码，在 load_balance 函数中，先会使用 need_active_balance 来判断是否真的需要执行 active balance：

```c++
static int need_active_balance(struct lb_env *env)
{
	struct sched_domain *sd = env->sd;

	if (env->idle == CPU_NEWLY_IDLE) {
		if ((sd->flags & SD_ASYM_PACKING) &&
		    sched_asym_prefer(env->dst_cpu， env->src_cpu))
			return 1;
	}

	if ((env->idle != CPU_NOT_IDLE) &&
	    (env->src_rq->cfs.h_nr_running == 1)) {
		if ((check_cpu_capacity(env->src_rq， sd)) &&
		    (capacity_of(env->src_cpu)*sd->imbalance_pct < capacity_of(env->dst_cpu)*100))
			return 1;
	}

	return unlikely(sd->nr_balance_failed > sd->cache_nice_tries+2);
}
```

从源码中不难看出，判断是否需要执行 active balance 分为几种情况：



* 如果 dst cpu 为 CPU_NEWLY_IDLE，如果 sd->flags 设置了 SD_ASYM_PACKING，表示该 domain 内的 CPU 是异构的，这时候就需要通过判断 dst_cpu 是否更适合运行进程来确定是否需要执行 active balance。  
* 第二种情况就是直接通过 capacity 来判断哪个 CPU 更适合运行进程，毕竟 capacity 更大的 CPU 执行进程更快，没理由让大核闲着小核来做事。  
* 进程迁移失败的次数很多，这是实在找不到合适的迁移进程了，同时也不能让当前 CPU 执行 idle 进程。通常 unlikely() 函数可以看出这种情况很少出现。    

执行 active balance 具体的操作函数为 stop_one_cpu_nowait，这个函数用于停止 CPU 上的进程，实际上是唤醒 CPU 上的内核线程，在该内核线程中执行传入的回调函数，在该回调函数中将原本正在执行的进程迁移到 dst CPU 上。 




## 小节

负载均衡在两种情况下执行：



* CPU 即将执行 idle 进程，这种情况下会从下到上遍历 domain 来找到合适的进程执行迁移，但是只需要迁移一个进程就心满意足地退出。 
* 周期性的负载均衡，由 tick 中断触发，这种情况下会先判断是否需要执行进程迁移，如果需要负载均衡同样是从下到上遍历 domain，找到合适的进程迁移到当前 CPU 上。   

理论上，每个 CPU 只会处理当前 CPU 处于相对空闲的情况，也就是把其它 busy CPU 上的进程移动到当前进程，存在[特殊情况](#jump)

同时，理论上正在运行的进程是不会被迁移的，但是当进程运行在不合适的 CPU 上，且更合适的 CPU 即将执行 idle 进程，这时候就会将正在执行的进程进行迁移。 



### 参考

4.9.88 内核源码

https://developer.aliyun.com/article/767294
https://www.kernel.org/doc/html/latest/x86/topology.html

https://www.ibm.com/developerworks/cn/linux/l-cn-schldom/
https://www.kernel.org/doc/html/latest/scheduler/sched-capacity.html

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。



