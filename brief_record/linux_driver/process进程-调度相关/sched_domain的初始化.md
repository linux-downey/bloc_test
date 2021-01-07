# linux sched_domain 的初始化
在前面的章节中，介绍了 CPU 拓扑结构以及 CPU 的初始化处理，CPU 的拓扑结构被保存在 cpu_topology 这个结构体数组中，包括 CPU 对应的 cluster_id,core_id 等，对于内核而言，CPU 和内存一样，也被看成一项硬件资源，它被动地接收调度器的安排，负责从内存中取出指令、执行指令、必要的时候将执行结果写回到内存。  

在单核系统中，只有一个逻辑 CPU，针对 CPU 的优化就是在有工作需要处理时尽量地不让它停下来，比如在 CPU 等待资源的时候切换到其它工作。在多核中的逻辑也是一样的，比如在一个简单的双核系统中，目标就是尽量地让两个核都不要停下来，除了单核下的那些优化方法之外，还涉及到另一个问题：如何让工作平均地分配到每个核上。  

系统中程序的最小执行单位为线程，也就是说调度器会将线程或者进程分配到 CPU 上执行，实际上，一个进程从诞生开始，通常都会在睡眠与唤醒之间来回切换，调度器无法预测哪些 CPU 上的进程将会睡眠而什么时候会被唤醒，可能某个 CPU 上的进程已经全部陷入睡眠而其它 CPU 上不堪重负，因此就涉及到一个优化问题：CPU 之间任务的负载均衡，也就是动态地将繁忙 CPU 上的任务迁移到相对空闲的 CPU 上。  

这个迁移的过程涉及到几个问题：
* 什么时候需要迁移？迁移发生在某些特定的场景下，比如 tick 中断、CPU 空闲时。 
* 如何确定哪个 CPU 繁忙而哪个 CPU 空闲？这个统计由调度器完成，调度器可以实时地记录 CPU 对应就绪队列上的负载。  
* 如何确定需要被迁移的任务？找到最繁忙的 CPU 的就绪队列和最空闲的就绪队列，从就绪队列上找到需要迁移的进程。
* 如何进行迁移？一旦确定了要迁移的任务，就直接将任务从源 CPU 的就绪队列上摘下来，放到目标就绪队列上。    

从理论上来说，这三点都是比较简单的，但是对于实际的实现，又涉及到很多非常繁琐的细节处理，比如繁忙和空闲的程度要达到一个什么样的比例才需要迁移？哪些进程不能被迁移？等等.  

在本章中，我们将重点讨论上文中的第一三四点，即确定迁移的进程以及迁移的具体操作，对于第二点 CPU 负载的计算并不详细讨论，这并不是进程迁移的重点。  


## 什么时候执行负载均衡?
负载均衡的触发实际上是非常频繁的，在 tick 中断中会尝试执行负载均衡，也就是内核会周期性地检查 CPU 之间的进程间是否平衡，tick 中断的周期由全局变量 HZ 决定，通常是 10ms 或者 4ms，对应的源码如下：

```c++
void scheduler_tick(void)
{
	...
	trigger_load_balance(rq);
}

void trigger_load_balance(struct rq *rq)
{
	if (time_after_eq(jiffies, rq->next_balance))
		raise_softirq(SCHED_SOFTIRQ);
}

```
执行负载均衡这项操作需要保证一定的间隔时间,太过于频繁地执行负载均衡其实是没有什么意义的,因此 CPU 的 rq->next_balance 用来设置下一次执行负载均衡的时间,这个时间是由调度域设置的,可以通过 /proc/sys/kernel/sched_domain/cpux/domainx/min_interval 和 max_interval 查看对应调度域设置的负载均衡最小和最大间隔时间,比如我的 ubuntu18 系统上的时间就是 2ms/4ms.  

当时间达到 rq->next_balance 时,将会触发软中断,执行负载均衡操作.  

同时，在一个 CPU 上没有可执行的进程时，也就是即将调度执行 idle 进程时，同样将触发负载均衡，这并不难理解，CPU 自己没工作需要执行了，自然要去别的 CPU 上看看有没有多余的工作可以转移过来，对应的内核函数为 idle_balance.  

上述的负载均衡执行的是整体上的负载均衡，而下面几种情况则是为即将运行的进程选择合适的 CPU，对系统的负载均衡有所贡献，但是并不算严格的负载均衡：
* 在执行 fork 创建子进程时，子进程被第一次调度运行时，将会选择一个合适的 CPU 执行。 
* 在进程发起 execve 系统调用时，将会选择一个合适的 CPU 执行。 
* 当进程被唤醒时，同样会选择合适的 CPU 执行。 

上面只是提到了负载均衡触发的时机以及对应的接口,其具体的源码实现我们将在后面讨论.   

## CPU 调度域
在 CPU 的负载均衡中有一个非常重要的概念:CPU 的调度域,在前面的章节中(TODO),介绍了 CPU 的拓扑结构以及缓存相关的知识,对于一个复杂的系统而言, CPU 的拓扑结构层次为 NUMA->socket(物理CPU)->cores->SMT,为什么分成这样的层次在该章节中有相应的介绍,从调度层面来看,同一个区域内的逻辑CPU之间迁移任务的开销是最小的,主要是出于缓存的考虑.   

对于同一个核心上的不同硬件线程来说,它们的大多数硬件资源都是共享的,core 内 core 外的所有级别的 cache 自然也是共享的,因此,将一个任务迁移到相同 core 的另一个硬件线程上,成本非常低.  

如果将任务在同一个 socket 中的不同 core 之间迁移,core 内的 cache(L1 为 per core,L2取决于处理器实现)是不共享的,而上层的 unify cache 和内存是共享的,将这些进程进行迁移的时候 core 内的 cache 需要重新填充,有一定的成本.  

实际上,NUMA 结构的实现并不只是一层,因为不想过多讨论 NUMA,因此我把它的概念简化了,它至少是有两部分的:一个 NUMA 系统包含多个 NUMA 节点,而一个 NUMA 节点也可以包含多个 socket,因此,跨 socket 的逻辑 CPU 之间的进程迁移只会共享本地内存或者 unify 内存部分,跨 NUMA 节点的进程迁移,可以想到,只共享最后一层 unify 内存.当然,迁移成本也是逐渐升高的.  

同时,上述所描述的 CPU 拓扑结构其实只是一个通用的概念,具体是什么结构取决于厂商的实现,而实现根据应用场景而定,通常是非常灵活的,比如多层次的大小核,非对称的物理 CPU 等等.我们所讨论的只是一种普遍情况,具体情况还需要参考对应的手册.   

### sched_domain
既然不同的 CPU 层级中进程迁移的成本是有区别的,这种区别自然也会在软件中有所体现,内核中使用 sched_domain 来描述调度域,调度域的抽象和硬件上的层级是类似的,一个调度域针对一个 cpu 拓扑结构的层级,系统中的调度域组成树状的结构,由 sched_domain 中的 parent 指针建立父级的索引关系.  

同一个调度域内,所有成员拥有相同的属性,比如一个 socket 中存在多个 core, 在 socket 调度域中,这些 core 共享 L3 cache(以及可能的 L2) 和内存资源,而在 core 所属的调度域内,所包含的硬件线程共享所有 level 的 cache 和内存,划分调度域的作用在于:在执行负载均衡时,以调度域为单位进行进程迁移,优先在调度域内处理,再依次向上处理高层级调度域,可以降低迁移的成本.同时也降低了软件的复杂度.    

### sched_group 
调度域的概念比较好理解,直接参考 CPU 的拓扑结构即可,但是在实际的负载均衡操作中,使用地更多的概念是调度组,有了调度域来区分 CPU 的层级为什么还需要调度组?   

组织了半天语言,发现没法儿通过文字的方式比较精确地表达 sched_domain 和 sched_group 之间的关系,还是通过示例和图来解释吧.  

假设在一个系统中,存在两个 socket,每个 socket 包含 4 个核心,而每个核心包含两个硬件线程,sched_group 和 sched_domain 的关系见下图(TODO):

从图中可以看出，因为包含两个 socket，因此顶层的 domain 包含两个子 domain，每个子 domain
对应一个 group，group 以循环链表的方式组织起来，sched_domain->groups 指向第一个 group。  

每个 group 的 cpumask 字段保存了子 domain 包含的所有 CPU，因此，在 Level3 domain 的 Grp0 包含 CPU0~cpu7 共 8 个逻辑 CPU，而 Grp1 类似。  

在 L2 domain 中，每个 socket 包含四个 core，也就是四个子 domain，由四个组表示，每个组包含两个逻辑 CPU，这里的逻辑 CPU 实际上是一个 core 上的两个硬件线程，依次类推。  

实际上，由于空间的原因，上图只是展示了 domain 与 group 之间的主要联系，实际上还有一些比较重要的信息：
* 在 L1 domain 中，由于空间原因将 Grp 信息省略了，实际上 Grp0/1 和 L2 domain 中一样，只是 L1 domain 中包含两个 group，每个 group 只包含一个逻辑 CPU。  
* L1、L2、L3 在图中为包含与被包含的关系，体现在软件上就是父子关系，由 sched_domain->parent 和 sched_domain->child 建立联系。 
* CPU domain 和 group 在内核中对应的数据结构为 sched_domain 和 sched_group，且组成树状的结构，需要注意的是，sched_domain 是 percpu 的，也就是每个 CPU 都保存了和自身相关的 domain 树状结构的分支，而 sched_group 可配置，默认是 CPU 间共享的。   

由此可知,domain 关注的是层次关系,而 group 则是对 CPU 的一种组织,在做系统的负载均衡时,domain 只是限定了负载均衡的区域,而 group 包含了需要处理的目标 CPU 集合,因此负载均衡中寻找最忙的 CPU 的操作通常是这样的:

```c++
for_each_domain(this_cpu, sd) {
	...
	group = find_busiest_group(&env);
	busiest_queue = find_busiest_queue(&env, group);
	...
}
```
从当前调度域向上递归到每个父级调度域,在每个调度域中,找到最忙的组,在组中找到最忙的 runqueue,也就是最忙的 CPU.  

理解了调度域和调度组之间的联系以及工作方式,负载均衡的难点几乎就已经解决了一半.  

## 选择合适的 CPU 执行进程
当一个进程新创建,调用 exec 或者被唤醒时,通常不需要考虑 cache 的问题(唤醒进程可能会有cache的残留),因此这时候将进程迁移到其它 CPU 并没有太多影响,选择合适的 CPU 通过 select_task_rq 函数,该函数会调用对应 sched_class->select_task_rq 回调函数,找到一个合适的 CPU 执行,最主要的影响因素是 CPU 的亲和性设置,其次再是取决于 CPU 的繁忙程度.  

对于经历睡眠然后被唤醒的进程而言,如果睡眠时间较短或者 CPU 并不太繁忙,是有可能该进程对应 CPU 的 cache 还没有被刷掉的,因此,如果是睡眠唤醒,需要考虑这个问题,也就是如果唤醒 CPU 与被唤醒进程之前所在的 CPU 在同一个低层的 domain 内(通常所有 CPU 是在同一个 top domain 内的,因此高层的 domain 意义不大),尽量在 domain 内选择 CPU,毕竟同一个 domain 内共享更多的 cache,然后再找到一个 idlest 的 CPU 来执行唤醒进程的.   

对于创建的和执行 exec 的进程,那就完全不需要考虑 cache 的问题了,毕竟这种进程都是全新的,不会在任何 CPU 上留下 cache 的足迹,因此,对于执行 CPU 的选择,就相对简单一点,不过也有需要注意的一点是:新执行的进程尽量和唤醒进程离得近一些,也就是能在执行唤醒进程低层的 domain 中找到 idlest cpu 就优先使用低层 domain,这样同样是出于 cache 的考虑,毕竟执行这些代码也会产生 cache 的替换,越低层 domain 之间 CPU 的操作就越可能产生可重用的 cache.   

## 负载均衡的执行
正如上文中提到的,系统中将在 tick 中断或者 CPU 无有效进程运行时触发负载均衡.  

需要注意的是,tick 中断的执行是 percpu 的,也就是说每个 CPU 是独立执行 tick 中断的,在执行负载均衡的时候也只会假设当前 CPU 处于比较空闲的状态,然后在各级 domain 中找到最 busy 的 CPU,当然,如果当前 CPU 并不空闲,也就没有必要执行负载均衡,那么,为什么这种负载均衡的操作不是在系统中找到最空闲的 CPU 和最 busy 的 CPU 直接操作呢?一方面,每个 CPU 都有机会自行进行负载均衡,另一方面,操作其它 CPU 很容易造成 rq lock 之间的竞争,同时产生一些无效的 cache,对系统来说是不利的,所以,对于负载均衡而言,都是各人自扫门前雪.   

当然,对于即将执行 idle 进程的 CPU 而言,已经确定了当前 CPU 即将处于空闲状态,需要做的就是找到最忙的 CPU 然后把任务迁移过来.  

确定了需要迁移的进程之后就会执行迁移,执行进程的迁移也需要考虑一些额外的细节,并不是所有情况下的进程都可以直接进行迁移:
* 如果这个进程正在 CPU 上执行,自然是不能迁移的(特殊情况有例外).
* 某些进程有 CPU 亲和性设置或者禁止了某些 CPU,进程迁移不能逾越这个规矩.  
* 如果进程即将被移出就绪队列,执行进程迁移也没什么意义,cfs 调度组的带宽控制会产生这个条件.  
* 如果进程的 cache 是热的,也不建议迁移,这并不是硬性限制,在进程迁移失败多次后可能会将其迁移.  

在找到可以迁移的进程之后,情况就变得比较简单了,直接对就绪队列上的进程执行 dequeue,然后 enqueue 到空闲 CPU 的队列上,也就完成了进程的迁移,当然,也有可能进程迁移失败,可能是系统中本来就没几个可迁移的进程,也可能是由于 CPU 亲和性的设置导致某个 CPU 被刻意地孤立,总之,负载均衡的工作到这里算是完成了.   


## 源码实现
在分析 cfs 调度器的时候,就有这样一个感慨:对于内核中的某些组件,其实现的核心思想并不复杂,但是其对应的实现确实充斥着大量的细节,如果不深入到源码中,就仅仅是了解一个大体的概念,这自然是不够的,只有分析过源代码之后,才能对该组件有一个更深刻的认识,除了更深刻的记忆之外,同样也可以看到内核源码设计的精妙所在,从而对内核有更深的理解.   

下面呢,就是源码的分析阶段,主要分为两个部分:
* sched_domain 和 sched_group 的构建
* 为新进程或者唤醒进程选择合适的 CPU
* 全局的负载均衡操作,tick 中断和即将执行 idle 进程这两种情况所执行的负载均衡有些区别,但最终都是执行 load_balance 这个主要的执行函数,因此放在一起讲,而且这是重点部分.  

### sched_domain 和 sched_group 的构建
在 start_kernel->rest_init->kernel_init->kernel_init_freeable 中,子函数 smp_prepare_cpus 负责通过扫描设备树或者从硬件中读取寄存器来构建 CPU 的拓扑结构,紧接着 smp_init 函数将会启动其它非 boot CPU,设置各个 CPU 的 present/online/active 掩码标志,以确定 CPU 的状态.  

然后,再调用 sched_init_smp 函数,从名字可以看出这是和调度相关的初始化工作,在该函数中,针对 numa 进行初始化,同时初始化 isolate map,这个 map 用于设置独立的 CPU,这些 CPU 不会被添加到调度域中,默认情况下进程不会被放到这些 CPU 上执行,除非进行人为的设置,最主要的工作在于初始化调度域和调度组,完成之后 CPU 就可以被调度器操作了.   

调度域的初始化对应的接口为:sched_init_domains(cpu_active_mask),传入的参数为 cpu_active_mask,也就是只会将 active 的 CPU 加入到调度域中,实际上,sched_domain 并非是固定的,在支持 CPU hotplug 的系统中,随着 CPU 拓扑结构的变化进行调整.  

int sched_init_domains(const struct cpumask *cpu_map)
{
	int err;
	...
	cpumask_andnot(doms_cur[0], cpu_map, cpu_isolated_map);
	err = build_sched_domains(doms_cur[0], NULL);
	register_sched_domain_sysctl();
	...
	return err;
}
在 sched_init_domains 中,cpu_map 表示 active 的 cpu 位图,需要 cpu_isolated_map 中记录的 CPU 排除,接着调用 build_sched_domains 建立整个系统的调度域,调度组也是在这个时候被构建.而 register_sched_domain_sysctl 这个函数用于将 sched_domain 相关的内核参数导出到用户空间的 /proc/sys 目录下,前提是 CONFIG_SCHED_DEBUG 和 CONFIG_SYSCTL 这两个内核选项被设置,对应的目录通常为:/proc/sys/kernel/sched_domain/cpux/domainx/.  

接下来看看最主要的一个接口 build_sched_domains,这个函数比较长,分为三个部分:
* 构建 sched_domain
* 在 sched_domain 的基础上构建 sched_group 
* attach CPU 和 sched_domain 

构建 sched_domain 的源码如下:

```c++
static int
build_sched_domains(const struct cpumask *cpu_map, struct sched_domain_attr *attr){
	...
	for_each_cpu(i, cpu_map) {
		struct sched_domain_topology_level *tl;

		sd = NULL;
		for_each_sd_topology(tl) {
			sd = build_sched_domain(tl, cpu_map, attr, sd, i);
			if (tl == sched_domain_topology)
				*per_cpu_ptr(d.sd, i) = sd;
			if (tl->flags & SDTL_OVERLAP)
				sd->flags |= SD_OVERLAP;
			if (cpumask_equal(cpu_map, sched_domain_span(sd)))
				break;
		}
	}
	...
}
```

从 for_each_cpu 的循环可以看出,sched_domain 的构建是 percpu 的.  

接下来的 struct sched_domain_topology_level 结构是比较重要的,从命名中的 level 中可以看出,这个结构用来描述 domain 域的层级,而 for_each_sd_topology 用来向上轮询当前 CPU 所属的所有 domain,源码如下:

```c++
static struct sched_domain_topology_level default_topology[] = {
#ifdef CONFIG_SCHED_SMT
	{ cpu_smt_mask, cpu_smt_flags, SD_INIT_NAME(SMT) },
#endif
#ifdef CONFIG_SCHED_MC
	{ cpu_coregroup_mask, cpu_core_flags, SD_INIT_NAME(MC) },
#endif
	{ cpu_cpu_mask, SD_INIT_NAME(DIE) },
	{ NULL, },
};

static struct sched_domain_topology_level *sched_domain_topology =
	default_topology;

#define for_each_sd_topology(tl)			\
	for (tl = sched_domain_topology; tl->mask; tl++)
```
从上面的源码可以看出,domain 的层级被静态定义在 default_topology 中,在 arm64 的代码中 domain 只有三层,最底层为 SMT,往上是 MC 和 DIE,对于 arm64(armv8架构) 平台而言,并不支持 SMT,因此只有两层 MC 和 DIE, MC 就是 Muti Core,这个比较好理解.而在 CPU 拓扑结构的介绍中,我们并没有提及到 DIE 这个概念,而是物理 CPU 或者 socket.  

实际上,DIE 是物理上的概念,是在芯片制作过程中实体的概念,它的概念和物理 CPU/socket 差不多,但是正如我们在 CPU 拓扑结构章节中提到的,实际上每个厂商制作芯片的步骤以及概念的定义并不一样,有些概念并不是完全地一一对应,取决于实际场景,这里将 DIE/物理CPU/socket 统一为一个概念并不完全精确,但是不影响我们理解软件上的概念.比如单单对于 x86 而言,多个 core 的集合通常被称为 package,在 AMD 中又称为 Node,实际上,die 是一个相对通用的概念,才会被 linux 内核采用.  

实际上不止是 arm64 架构,即使在主线代码中,内核的 domain 层级结构并没有看到 NUMA domain 的身影,可能 NUMA domain 可以通过其它方式进行扩展,或者 NUMA 系统需要自行在该静态数组中增加对应的 domain level 项.   

有一个问题是,这些层级是根据什么来定义的呢?可以看到 default_topology 中每一项的 mask 部分,cpu_coregroup_mask 和 cpu_cpu_mask 的定义分别为:

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
从源码可以看出,对于 MC domain 而言,包含当前 CPU 的 core_silbling,而对于 DIE domain,则是当前 NUMA node 中的 CPU,虽然不是 NUMA 结构系统,但是可以把整个系统当成一个单独的 NUMA node,一个 NUMA node 包含多个 DIE 这个概念是可以使用的.  

因此,domain 的构建完全是基于 CPU topology 的,这也是为什么需要花两个章节来专门介绍 CPU topology 的原因.   

构建 domain 由 build_sched_domain 完成,这个函数被包含在 for_each_cpu 和 for_each_sd_topology 中,因此,会针对每个 CPU 的每个 domain level 建立 sched_domain,然后通过 child 和 parent 指针将它们建立联系,因为 sched_domain 是 percpu 的,所以 parent 和 child 都只有一个,因此对于每个 CPU 而言, sched_domain 并不是树状结构,而是链式结构.在 build_sched_domain 中,会调用 sd_init 初始化一个全新的 sched_domain,同时设置 sched_domain 的 span 和 span_weight,一个是当前 domain 的 CPU mask,一个是 cpu 数量,在调度时会用到.     

每构建完一层 domain 结构,都会调用 if (cpumask_equal(cpu_map, sched_domain_span(sd))) 来检查当前层级的 CPU mask是否与所有 active cpu 的 mask 一致,如果一致,就不再构建上层的 sched_domain.举个例子,在一个单 socket 包含 4 multi core 的系统中,构建的 MC domain 就包含了 4 个 CPU,其实这时候就没必要再为单个 socket 再构建一层 DIE domain 了,即使构建了,该 DIE domain 中也只包含一个子 domain,一个 group,实际上是没什么意义的,只会导致在做负载均衡的时候白白增加一次遍历成本,因此,单 socket 的 arm 系统中(不支持 SMT)通常只有一层 MC domain 域.   


### select_task_rq —— 选择进程 CPU
select_task_rq 是核心调度部分的接口,不针对具体的调度器类,在该函数中，主要是针对进程相对 CPU 亲和性的处理，在 task_struct 结构中，有两个相关的字段：nr_cpus_allowed 和 cpus_allowed，nr_cpus_allowed 表示当前进程可以运行的 CPU 数量，而 cpus_allowed 是当前进程可运行 CPU 的掩码位，比如进程 p 可运行在 0,2,3 CPU 上，nr_cpus_allowed 为 3 而 3，cpus_allowed 为 0b00001101。  

当进程的 nr_cpus_allowed 不大于 1 时，通常表示该进程是绑定 CPU 的，因此这一类进程就不需要选择 CPU，对于其它进程，则调用对应调度器类的 select_task_rq 回调函数，该函数会返回合适执行该进程的 CPU id。  

对于 cfs 调度器，对应的 select_task_rq 回调函数为 select_task_rq_fair：

```c++
select_task_rq_fair(struct task_struct *p, int prev_cpu, int sd_flag, int wake_flags)
{
	...
	if (sd_flag & SD_BALANCE_WAKE) {
		record_wakee(p);
		want_affine = !wake_wide(p) && !wake_cap(p, cpu, prev_cpu)   .............................1
			      && cpumask_test_cpu(cpu, &p->cpus_allowed);
	}
	for_each_domain(cpu, tmp) {
		if (want_affine && (tmp->flags & SD_WAKE_AFFINE) &&          ..............................2
		    cpumask_test_cpu(prev_cpu, sched_domain_span(tmp))) {
			affine_sd = tmp;
			break;
		}
	}
	if (affine_sd) {
		sd = NULL; 
		if (cpu == prev_cpu)
			goto pick_cpu;

		if (wake_affine(affine_sd, p, prev_cpu, sync))
			new_cpu = cpu;
	}
	if (!sd) {
 pick_cpu:
		if (sd_flag & SD_BALANCE_WAKE) 
			new_cpu = select_idle_sibling(p, prev_cpu, new_cpu);         ........................3
	}
	...
}
```
传入第一个参数为需要操作的 task，第二个参数为 task 之前运行的进程，后面是两个 flag 参数,在 select_task_rq_fair 的前面一部分是针对唤醒进程的操作。  

注1：want_affine 这个变量从字面上的理解是是否想设置唤醒的亲和性，是不是需要让进程在本 domain 中的 CPU 上执行，这个变量成立是有条件的，包括当前 CPU 的 capacity 是否和之前 CPU 的容量(这个概念在后面讲到)差不多，当前 CPU 是否在进程的 cpus_allowed 中，同时 wake_wide(p) 返回 0，说起来这个 wake_wide 是有些复杂的，理论上来说，让进程在本地被唤醒从 cache 考虑是有益的，但是对于某些生产消费模型中，可能会出现生产进程频繁唤醒多个消费进程，出于消费进程执行效率的考虑，这种情况下最好将多个消费进程分散到多个 CPU 中快速执行完，而不是挤在一个繁忙的 CPU 上，在内核中使用 wakee_flips 来记录这种唤醒行为，通过该变量可以判断是否处于这种模型下，而特意选择较远的 CPU 执行，从而不考虑亲和性。  

注2：如果设置了 want_affine 变量，表示需要就近选择合适的 CPU，同时，如果待唤醒进程之前执行的进程也在当前 domain 下，就选中当前 domain，否则向上遍历到父级 domain。  

注3：确定了 domain，接下来的工作也就是从 domain 中找到一个合适的 CPU，这个工作由 select_idle_sibling 完成，这个函数会在共享 LLC 的 domain 内，分别调用 select_idle_core、select_idle_cpu、select_idle_smt 以找到最合适的 CPU。  


如果不是唤醒的进程，或者唤醒进程不设置亲和 CPU，那么就进入到通用的选择 CPU 的流程：

```c++
static int
select_task_rq_fair(struct task_struct *p, int prev_cpu, int sd_flag, int wake_flags)
{
	...
	while (sd) {                             ..............................................1
		struct sched_group *group;
		int weight;

		if (!(sd->flags & sd_flag)) {
			sd = sd->child;
			continue;
		}

		group = find_idlest_group(sd, p, cpu, sd_flag);    
		if (!group) {
			sd = sd->child;
			continue;
		}

		new_cpu = find_idlest_cpu(group, p, cpu);            .................................2
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


## idle_balance







参考：https://developer.aliyun.com/article/767294




