## linux调度子系统14 - 触发负载均衡场景

在分析 cfs 调度器的时候，就有这样一个感慨:对于内核中的某些组件，其实现的核心思想并不复杂，但是其对应的实现确实充斥着大量的细节，如果不深入到源码中，就仅仅是了解一个大体的概念，这自然是不够的，只有分析过源代码之后，才能对该组件有一个更深刻的认识，除了更深刻的记忆之外，同样也可以看到内核源码设计的精妙所在，从而对内核有更深的理解.   

下面呢，就是源码的分析阶段，主要分为几个部分:



* sched_domain 和 sched_group 的构建
* 为新进程或者唤醒进程选择合适的 CPU
* 全局的负载均衡操作，tick 中断和即将执行 idle 进程这两种情况所执行的负载均衡有些区别，但最终都是执行 load_balance 这个主要的执行函数，因此放在一起讲，而且这是重点部分.  



### sched_domain 和 sched_group 的构建

在 start_kernel->rest_init->kernel_init->kernel_init_freeable 中，子函数 smp_prepare_cpus 负责通过扫描设备树或者从硬件中读取寄存器来构建 CPU 的拓扑结构，紧接着 smp_init 函数将会启动其它非 boot CPU，设置各个 CPU 的 present/online/active 掩码标志，以确定 CPU 的状态.  

然后，再调用 sched_init_smp 函数，从名字可以看出这是和调度相关的初始化工作，在该函数中，针对 numa 进行初始化，同时初始化 isolate map，这个 map 用于设置独立的 CPU，这些 CPU 不会被添加到调度域中，默认情况下进程不会被放到这些 CPU 上执行，除非进行人为的设置，最主要的工作在于初始化调度域和调度组，完成之后 CPU 就可以被调度器操作了.   

调度域的初始化对应的接口为:sched_init_domains(cpu_active_mask)，传入的参数为 cpu_active_mask，也就是只会将 active 的 CPU 加入到调度域中，实际上，sched_domain 并非是固定的，在支持 CPU hotplug 的系统中，随着 CPU 拓扑结构的变化进行调整.  

```c++
int sched_init_domains(const struct cpumask *cpu_map)
{
	int err;
	...
	cpumask_andnot(doms_cur[0]， cpu_map， cpu_isolated_map);
	err = build_sched_domains(doms_cur[0]， NULL);
	register_sched_domain_sysctl();
	...
	return err;
}
```


在 sched_init_domains 中，cpu_map 表示 active 的 cpu 位图，需要 cpu_isolated_map 中记录的 CPU 排除，接着调用 build_sched_domains 建立整个系统的调度域，调度组也是在这个时候被构建.而 register_sched_domain_sysctl 这个函数用于将 sched_domain 相关的内核参数导出到用户空间的 /proc/sys 目录下，前提是 CONFIG_SCHED_DEBUG 和 CONFIG_SYSCTL 这两个内核选项被设置，对应的目录通常为:/proc/sys/kernel/sched_domain/cpux/domainx/.  

接下来看看最主要的一个接口 build_sched_domains，这个函数比较长，分为三个部分:



* 构建 sched_domain
* 在 sched_domain 的基础上构建 sched_group 

构建 sched_domain 的源码如下:

```c++
static int
build_sched_domains(const struct cpumask *cpu_map， struct sched_domain_attr *attr){
	...
	for_each_cpu(i， cpu_map) {
		struct sched_domain_topology_level *tl;

		sd = NULL;
		for_each_sd_topology(tl) {
			sd = build_sched_domain(tl， cpu_map， attr， sd， i);
			if (tl == sched_domain_topology)
				*per_cpu_ptr(d.sd， i) = sd;
			if (tl->flags & SDTL_OVERLAP)
				sd->flags |= SD_OVERLAP;
			if (cpumask_equal(cpu_map， sched_domain_span(sd)))
				break;
		}
	}
	...
}
```

从 for_each_cpu 的循环可以看出，sched_domain 的构建是 percpu 的.  

接下来的 struct sched_domain_topology_level 结构是比较重要的，从命名中的 level 中可以看出，这个结构用来描述 domain 域的层级，而 for_each_sd_topology 用来向上轮询当前 CPU 所属的所有 domain，源码如下:

```c++
static struct sched_domain_topology_level default_topology[] = {
#ifdef CONFIG_SCHED_SMT
	{ cpu_smt_mask， cpu_smt_flags， SD_INIT_NAME(SMT) }，
#endif
#ifdef CONFIG_SCHED_MC
	{ cpu_coregroup_mask， cpu_core_flags， SD_INIT_NAME(MC) }，
#endif
	{ cpu_cpu_mask， SD_INIT_NAME(DIE) }，
	{ NULL， }，
};

static struct sched_domain_topology_level *sched_domain_topology =
	default_topology;

#define for_each_sd_topology(tl)			\
	for (tl = sched_domain_topology; tl->mask; tl++)
```

从上面的源码可以看出，domain 的层级被静态定义在 default_topology 中，在 arm64 的代码中 domain 只有三层，最底层为 SMT，往上是 MC 和 DIE，对于 arm64(armv8架构) 平台而言，并不支持 SMT，因此只有两层 MC 和 DIE， MC 就是 Muti Core，这个比较好理解.而在 CPU 拓扑结构的介绍中，我们并没有提及到 DIE 这个概念，而是物理 CPU 或者 socket.  

实际上，DIE 是物理上的概念，是在芯片制作过程中实体的概念，它的概念和物理 CPU/socket 差不多，但是正如我们在 CPU 拓扑结构章节中提到的，实际上每个厂商制作芯片的步骤以及概念的定义并不一样，有些概念并不是完全地一一对应，取决于实际场景，这里将 DIE/物理CPU/socket 统一为一个概念并不完全精确，但是不影响我们理解软件上的概念.比如单单对于 x86 而言，多个 core 的集合通常被称为 package，在 AMD 中又称为 Node，实际上，die 是一个相对通用的概念，才会被 linux 内核采用.  

实际上不止是 arm64 架构，即使在主线代码中，内核的 domain 层级结构并没有看到 NUMA domain 的身影，可能 NUMA domain 可以通过其它方式进行扩展，或者 NUMA 系统需要自行在该静态数组中增加对应的 domain level 项.   

有一个问题是，这些层级是根据什么来定义的呢?可以看到 default_topology 中每一项的 mask 部分，cpu_coregroup_mask 和 cpu_cpu_mask 的定义分别为:

```c++
const struct cpumask *cpu_coregroup_mask(int cpu)
{
	return &cpu_topology[cpu].core_sibling;
}
static inline const struct cpumask *cpu_cpu_mask(int cpu)
{
	return cpumask_of_node(cpu_to_node(cpu));
}
```

从源码可以看出，对于 MC domain 而言，包含当前 CPU 的 core_silbling，而对于 DIE domain，则是当前 NUMA node 中的 CPU，虽然不是 NUMA 结构系统，但是可以把整个系统当成一个单独的 NUMA node，一个 NUMA node 包含多个 DIE 这个概念是可以使用的.  

因此，domain 的构建完全是基于 CPU topology 的，这也是为什么需要花两个章节来专门介绍 CPU topology 的原因.   

构建 domain 由 build_sched_domain 完成，这个函数被包含在 for_each_cpu 和 for_each_sd_topology 中，因此，会针对每个 CPU 的每个 domain level 建立 sched_domain，然后通过 child 和 parent 指针将它们建立联系，因为 sched_domain 是 percpu 的，所以 parent 和 child 都只有一个，因此对于每个 CPU 而言， sched_domain 并不是树状结构，而是链式结构.在 build_sched_domain 中，会调用 sd_init 初始化一个全新的 sched_domain，同时设置 sched_domain 的 span 表示当前 domain 的 CPU mask.      

每构建完一层 domain 结构，都会调用 if (cpumask_equal(cpu_map， sched_domain_span(sd))) 来检查当前层级的 CPU mask是否与所有 active cpu 的 mask 一致，如果一致，就不再构建上层的 sched_domain.

举个例子，在一个单 socket 包含 4 multi core 的系统中，构建的 MC domain 就包含了 4 个 CPU，其实这时候就没必要再为单个 socket 再构建一层 DIE domain 了，即使构建了，该 DIE domain 中也只包含一个子 domain，一个 group，实际上是没什么意义的，只会导致在做负载均衡的时候白白增加一次遍历成本，因此，单 socket 的 arm 系统中(不支持 SMT)通常只有一层 MC domain 域.   

build_sched_domains 的第二部分是构建 sched_group，源码如下：

```c++
static int
build_sched_domains(const struct cpumask *cpu_map， struct sched_domain_attr *attr)
{
	...
	for_each_cpu(i， cpu_map) {
		for (sd = *per_cpu_ptr(d.sd， i); sd; sd = sd->parent) {
			sd->span_weight = cpumask_weight(sched_domain_span(sd));
			...
			if (build_sched_groups(sd， i))
				goto error;
			...
		}
	}
	...
}
```

针对每个层级的 domain，给 sd 的 span_weight 赋值，该值表示当前 domain 内的 CPU 数量。  

build_sched_groups 负责构建 percpu 的 sched_group，主要是设置 sg->ref 引用计数，以及 sg->cpumask，该值主要包括当前组内所有 CPU 的 mask，同时，还初始化 sg 的 capacity。  

CPU 的 capacity 代表了一个 CPU 执行程序的能力，capacity 越大，执行程序的效率越高。  






## idle_balance

当某个 CPU 的调度器在就绪队列上找到有效的进程时，就会看看其它 CPU 上是否有多的进程，并拿过来执行，如果确实没有，才会不甘心地执行 idle 进程，这时候发起的负载均衡就是 idle_balance.  

相对于周期性的负载均衡而言，idle balance 的目的更明确，调用 load_balance 的 flag 参数为 CPU_NEWLY_IDLE，表示当前进程已经没有进程可执行了，属于最紧急的负载均衡情况。  

下面是 idle_balance 的源码实现：

```c++
static int idle_balance(struct rq *this_rq， struct rq_flags *rf)
{
	...
	unsigned long next_balance = jiffies + HZ;
	if (this_rq->avg_idle < sysctl_sched_migration_cost ||    ......1
	    !this_rq->rd->overload) {
		sd = rcu_dereference_check_sched_domain(this_rq->sd);
		if (sd)
			update_next_balance(sd， &next_balance);
		goto out;
	}

	for_each_domain(this_cpu， sd) {
		int continue_balancing = 1;
		u64 t0， domain_cost;

		if (this_rq->avg_idle < curr_cost + sd->max_newidle_lb_cost) {  ........2
			update_next_balance(sd， &next_balance);
			break;
		}

		if (sd->flags & SD_BALANCE_NEWIDLE) {
			t0 = sched_clock_cpu(this_cpu);

			pulled_task = load_balance(this_cpu， this_rq，
						   sd， CPU_NEWLY_IDLE，
						   &continue_balancing);

			domain_cost = sched_clock_cpu(this_cpu) - t0;
			if (domain_cost > sd->max_newidle_lb_cost)
				sd->max_newidle_lb_cost = domain_cost;

			curr_cost += domain_cost;
		}

		update_next_balance(sd， &next_balance);

		if (pulled_task || this_rq->nr_running > 0)         ..........3
			break;
	}
}

```

注1：执行 idle_balance 的根本目的在于提高程序的执行效率，迁移进程只是一种实现手段，当迁移进程本身的开销大于进程迁移产生的收益时，是不应该迁移进程的，因此，在 idle_balance 的前部分，会判断当前 CPU 的 rq 进入 idle 状态的平均时间，如果这个时间要小于进程迁移所产生的开销，就说明很大概率该 CPU 上马上就会有进程被唤醒，这时候做进程迁移就显得没那么必要了，同时，另一个条件是：如果系统中的负载本来就很轻，甚至每个 CPU 上只有或者不到一个进程，这时候下面的代码也没有太多必要执行了。在这两种情况下，直接更新一下下次执行 balance 的时间，退出即可。  

注2：确定需要执行进程迁移只有，使用 for_each_domain 从当前 domain 向上遍历到顶层 domain，调用 load_balance 试图寻找合适的可迁移的进程，但是这个过程的开销必须是可控的，为了负载均衡的操作花去太多时间并不划算，因此每执行一次 load_balance 就会记录一次时间开销，当这个开销达到一定的值，就不再继续执行负载均衡

注3：idle_balance 的目的就在于让当前 CPU 有进程可运行，并不用太贪心，因此如果有进程被迁移过来或者当前 CPU 上有进程被唤醒都会推出负载均衡的过程。  

负载均衡最核心的部分还是 load_balance 函数，这个我们将在后续统一分析。  




## trigger_load_balance

在周期性的 tick 中，同样执行负载均衡的操作，对应的函数为 trigger_load_balance，这个接口将会触发 SCHED_SOFTIRQ 软中断，该软中断对应的执行函数为 run_rebalance_domains。  

run_rebalance_domains 和 idle_balance 不同的是，传入的 flag 参数根据当前 CPU 是否处于 idle 分为 CPU_IDLE 或者 CPU_NOT_IDLE，同时，根据 sd 的对应的 balance intercal 属性确定下次执行 balance 的时间，记录 last_balance。  

主要的部分还是从当前 domain 向上遍历，调用 load_balance，是否真的需要执行进程迁移由 load_balance 函数进行判断。   



### select_task_rq —— 选择进程 CPU

select_task_rq 是核心调度部分的接口，不针对具体的调度器类，在该函数中，主要是针对进程相对 CPU 亲和性的处理，在 task_struct 结构中，有两个相关的字段：nr_cpus_allowed 和 cpus_allowed，nr_cpus_allowed 表示当前进程可以运行的 CPU 数量，而 cpus_allowed 是当前进程可运行 CPU 的掩码位，比如进程 p 可运行在 0，2，3 CPU 上，nr_cpus_allowed 为 3 而 3，cpus_allowed 为 0b00001101。  

当进程的 nr_cpus_allowed 不大于 1 时，通常表示该进程是绑定 CPU 的，因此这一类进程就不需要选择 CPU，对于其它进程，则调用对应调度器类的 select_task_rq 回调函数，该函数会返回合适执行该进程的 CPU id。  

对于 cfs 调度器，对应的 select_task_rq 回调函数为 select_task_rq_fair：

```c++
select_task_rq_fair(struct task_struct *p， int prev_cpu， int sd_flag， int wake_flags)
{
	...
	if (sd_flag & SD_BALANCE_WAKE) {
		record_wakee(p);
		want_affine = !wake_wide(p) && !wake_cap(p， cpu， prev_cpu)   .............................1
			      && cpumask_test_cpu(cpu， &p->cpus_allowed);
	}
	for_each_domain(cpu， tmp) {
		if (want_affine && (tmp->flags & SD_WAKE_AFFINE) &&          ..............................2
		    cpumask_test_cpu(prev_cpu， sched_domain_span(tmp))) {
			affine_sd = tmp;
			break;
		}
	}
	if (affine_sd) {
		sd = NULL; 
		if (cpu == prev_cpu)
			goto pick_cpu;

		if (wake_affine(affine_sd， p， prev_cpu， sync))
			new_cpu = cpu;
	}
	if (!sd) {
 pick_cpu:
		if (sd_flag & SD_BALANCE_WAKE) 
			new_cpu = select_idle_sibling(p， prev_cpu， new_cpu);         ........................3
	}
	...
}
```

传入第一个参数为需要操作的 task，第二个参数为 task 之前运行的进程，后面是两个 flag 参数，在 select_task_rq_fair 的前面一部分是针对唤醒进程的操作。  



注1：want_affine 这个变量从字面上的理解是是否想设置唤醒的亲和性，是不是需要让进程在本 domain 中的 CPU 上执行，这个变量成立是有条件的，包括当前 CPU 的 capacity 是否和之前 CPU 的容量(这个概念在后面讲到)差不多，当前 CPU 是否在进程的 cpus_allowed 中，同时 wake_wide(p) 返回 0，说起来这个 wake_wide 是有些复杂的，理论上来说，让进程在本地被唤醒从 cache 考虑是有益的，但是对于某些生产消费模型中，可能会出现生产进程频繁唤醒多个消费进程，出于消费进程执行效率的考虑，这种情况下最好将多个消费进程分散到多个 CPU 中快速执行完，而不是挤在一个繁忙的 CPU 上，在内核中使用 wakee_flips 来记录这种唤醒行为，通过该变量可以判断是否处于这种模型下，而特意选择较远的 CPU 执行，从而不考虑亲和性。  

注2：如果设置了 want_affine 变量，表示需要就近选择合适的 CPU，同时，如果待唤醒进程之前执行的进程也在当前 domain 下，就选中当前 domain，否则向上遍历到父级 domain。  

注3：确定了 domain，接下来的工作也就是从 domain 中找到一个合适的 CPU，这个工作由 select_idle_sibling 完成，这个函数会在共享 LLC 的 domain 内，分别调用 select_idle_core、select_idle_cpu、select_idle_smt 以找到最合适的 CPU。  


如果不是唤醒的进程，或者唤醒进程不设置亲和 CPU，那么就进入到通用的选择 CPU 的流程：

```c++
static int
select_task_rq_fair(struct task_struct *p， int prev_cpu， int sd_flag， int wake_flags)
{
	...
	while (sd) {                             .........................1
		struct sched_group *group;
		int weight;

		if (!(sd->flags & sd_flag)) {
			sd = sd->child;
			continue;
		}

		group = find_idlest_group(sd， p， cpu， sd_flag);    
		if (!group) {
			sd = sd->child;
			continue;
		}

		new_cpu = find_idlest_cpu(group， p， cpu);       .........2
		if (new_cpu == -1 || new_cpu == cpu) {
			sd = sd->child;
			continue;
		}

		...
	}
	return new_cpu;
	...
}
```

注1：在上文判断是否亲和唤醒时，使用了 for_each_domain 循环，如果不是亲和唤醒，最终会遍历到最上层 domain，sd 的初始化则是最上层的 domain，因此，和其它 domain 遍历操作不一样的是，这里的 domain 是从下往上遍历的，在每一层 domain 寻找 idlest 的 CPU，与当前 CPU 共享更底层 domain 的 CPU 获得优先权。  

注2：寻找 idlest CPU 先后调用了 find_idlest_group 和 find_idlest_cpu 函数，从名称可以看出它们的作用，在调度器工作的过程中就一直记录着 CPU rq 的负载情况，因此通过这些负载情况可以找到 idlest 或者 busiest cpu，实际上，CPU 负载的统计并不简单，有机会的话后面会单独讲解，这里不过多赘述。  

在最后，将会返回合适的 CPU，可以发现，CPU 的选择基本遵循的规则为：考虑进程设置的 CPU 亲和性、优先本地化唤醒。  



### 参考

4.9.88 内核源码

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。

