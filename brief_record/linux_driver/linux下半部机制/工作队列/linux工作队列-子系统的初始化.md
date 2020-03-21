# linux 工作队列 - 子系统的初始化

workqueue 初始化的开始有两个函数：
* workqueue_init_early
* workqueue_init

workqueue_init_early 直接在 start_kernel 中被调用，而 workqueue_init 的调用路径为：

```
start_kernel
	rest_init
		kernel_init
			kernel_init_freeable
				workqueue_init
```

从函数名可以看出，workqueue_init_early 负责前期的初始化工作，我们就看看这个接口：

```c
int __init workqueue_init_early(void)
{
	int std_nice[NR_STD_WORKER_POOLS] = { 0, HIGHPRI_NICE_LEVEL };
	int i, cpu;

	WARN_ON(__alignof__(struct pool_workqueue) < __alignof__(long long));

	BUG_ON(!alloc_cpumask_var(&wq_unbound_cpumask, GFP_KERNEL));
	cpumask_copy(wq_unbound_cpumask, cpu_possible_mask);

	pwq_cache = KMEM_CACHE(pool_workqueue, SLAB_PANIC);

	for_each_possible_cpu(cpu) {
		struct worker_pool *pool;

		i = 0;
		for_each_cpu_worker_pool(pool, cpu) {
			BUG_ON(init_worker_pool(pool));
			pool->cpu = cpu;
			cpumask_copy(pool->attrs->cpumask, cpumask_of(cpu));
			pool->attrs->nice = std_nice[i++];
			pool->node = cpu_to_node(cpu);

			mutex_lock(&wq_pool_mutex);
			BUG_ON(worker_pool_assign_id(pool));
			mutex_unlock(&wq_pool_mutex);
		}
	}

	/* create default unbound and ordered wq attrs */
	for (i = 0; i < NR_STD_WORKER_POOLS; i++) {
		struct workqueue_attrs *attrs;

		BUG_ON(!(attrs = alloc_workqueue_attrs(GFP_KERNEL)));
		attrs->nice = std_nice[i];
		unbound_std_wq_attrs[i] = attrs;

		BUG_ON(!(attrs = alloc_workqueue_attrs(GFP_KERNEL)));
		attrs->nice = std_nice[i];
		attrs->no_numa = true;
		ordered_wq_attrs[i] = attrs;
	}

	system_wq = alloc_workqueue("events", 0, 0);
	system_highpri_wq = alloc_workqueue("events_highpri", WQ_HIGHPRI, 0);
	system_long_wq = alloc_workqueue("events_long", 0, 0);
	system_unbound_wq = alloc_workqueue("events_unbound", WQ_UNBOUND,
					    WQ_UNBOUND_MAX_ACTIVE);
	system_freezable_wq = alloc_workqueue("events_freezable",
					      WQ_FREEZABLE, 0);
	system_power_efficient_wq = alloc_workqueue("events_power_efficient",
					      WQ_POWER_EFFICIENT, 0);
	system_freezable_power_efficient_wq = alloc_workqueue("events_freezable_power_efficient",
					      WQ_FREEZABLE | WQ_POWER_EFFICIENT,
					      0);
	BUG_ON(!system_wq || !system_highpri_wq || !system_long_wq ||
	       !system_unbound_wq || !system_freezable_wq ||
	       !system_power_efficient_wq ||
	       !system_freezable_power_efficient_wq);

	return 0;
}

```

第一阶段的初始化分为以下几个主要部分：
* 创建 pool_workqueue 的高速缓存池
* 初始化 percpu 类型的 worker pool
* 创建各种类型的工作队列

## 创建高速缓存池
在内核中，通常使用 kmalloc 进行内存的分配，这种分配方式小巧灵活：需要用到的时候就向系统申请，不需要使用了就调用对应的 free 函数，系统将其回收。  

但是，对于频繁申请的同一类型数据，系统大可不必每次都将其回收，鉴于频繁使用的特性，系统其 free 的时候，可以将它暂存起来，下次再申请的时候，系统就直接返回上次使用的那片内存，这样可以明显地提高效率，毕竟频繁的内存分配工作是非常耗时的。  

这种缓存的做法在内核中以 kmem_cache 的方式提供，主要提供以下接口：

```
struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t offset,
	                                unsigned long flags, void (*ctor)(void *))
```
kmem_cache_create 用于创建一个指定 size 的缓存，这个 size 通过对应需要使用的数据结构的大小，这个缓存是一片保留内存，通常并不直接使用，而是在当前缓存的基础上调用 kmem_cache_alloc，返回内存指针，再对该指针进行读写操作。  

```
void *kmem_cache_alloc(struct kmem_cache *, gfp_t flags)
```

在当前初始化函数中，使用 KMEM_CACHE 接口创建缓存结构，这个接口是基于 kmem_cache_create 的封装：

```
#define KMEM_CACHE(__struct, __flags) kmem_cache_create(#__struct,\
		sizeof(struct __struct), __alignof__(struct __struct),\
		(__flags), NULL)
```

## 初始化 percpu 类型的 worker pool
在当前文件的头部，静态地创建了 percpu 类型的 worker pool 结构：

```
static DEFINE_PER_CPU_SHARED_ALIGNED(struct worker_pool [NR_STD_WORKER_POOLS], cpu_worker_pools);
```

其中，NR_STD_WORKER_POOLS 的值默认为 2，也就是系统初始化时每个 cpu 定义两个静态的 worker pool，上文中贴出的初始化的代码简化出来就是：

```

for_each_possible_cpu{                 //遍历 cpu 操作
    for_each_cpu_worker_pool{          //遍历 worker pool 操作
        init_worker_pool(pool);        //初始化 worker pool
        pool->cpu = cpu;               //绑定 worker pool 和 cpu
        cpumask_copy(pool->attrs->cpumask, cpumask_of(cpu));  //获取 cpu mask
        pool->node = cpu_to_node(cpu);  //获取 numa node 节点
        worker_pool_assign_id(pool);    //为当前 worker pool 分配 id 号
    }
}
    
```
其中，前面的两个 for_each 嵌套表示：以下的操作针对每个 cpu 下的每个 worker_pool。    

init_worker_pool 负责初始化 worker_pool:

```c++
static int init_worker_pool(struct worker_pool *pool)
{
	pool->id = -1;
	pool->cpu = -1;
	pool->node = NUMA_NO_NODE;
	pool->flags |= POOL_DISASSOCIATED;
	pool->watchdog_ts = jiffies;
	INIT_LIST_HEAD(&pool->worklist);
	INIT_LIST_HEAD(&pool->idle_list);
	hash_init(pool->busy_hash);

	setup_deferrable_timer(&pool->idle_timer, idle_worker_timeout,
			       (unsigned long)pool);

	setup_timer(&pool->mayday_timer, pool_mayday_timeout,
		    (unsigned long)pool);

	INIT_LIST_HEAD(&pool->workers);

	ida_init(&pool->worker_ida);
	INIT_HLIST_NODE(&pool->hash_node);
	pool->refcnt = 1;

	pool->attrs = alloc_workqueue_attrs(GFP_KERNEL);
}
```

按照阅读内核代码的经验来说，init 操作通常就是对结构体中的成员进行初始化，init_worker_pool 也不例外，其中值得注意的有几个成员：
* pool->node: NUMA node，这涉及到 numa 架构的概念，numa 存在于 SMP 系统中，通俗地说就是：

    在 SMP 系统中，多个 cpu 核同时访问系统中共用的内存会出现总线争用和高速缓存效率的问题，所以将整片内存分开连接到不同的 cpu，每个 cpu 核尽可能使用 local ram，不同的 local ram 之间也可以交换数据，这种方式成为了一种解决方案，应用这种方案的系统就称为 numa 系统，这里的 cpu 核与内存组成了一个 numa node。当然，SMP 结构中也可以不使用 noma 架构。  

* pool->attrs = alloc_workqueue_attrs(GFP_KERNEL)：初始化 pool atrrs(属性),该属性包含三个部分：
    * nice：优先级
    * cpumask：cpu 掩码
    * no_numa:标记是否处于 numa 架构。  


初始化完成之后，就是对pool->cpu、pool->node 、cpumask 等成员进行赋值，同时 调用 worker_pool_assign_id 分配 worker_pool 的 id，该 id 由 idr 数据结构进行管理。  


## 创建各种类型的工作队列
初始化完成 worker_pool 之后，内核通过传入不同的 flag 多次调用 alloc_workqueue 创建了大量的工作队列:

```c++
system_wq = alloc_workqueue("events", 0, 0);
system_highpri_wq = alloc_workqueue("events_highpri", WQ_HIGHPRI, 0);
system_long_wq = alloc_workqueue("events_long", 0, 0);
system_unbound_wq = alloc_workqueue("events_unbound", WQ_UNBOUND,WQ_UNBOUND_MAX_ACTIVE);
system_freezable_wq = alloc_workqueue("events_freezable",WQ_FREEZABLE, 0);
system_power_efficient_wq = alloc_workqueue("events_power_efficient",WQ_POWER_EFFICIENT, 0);
system_freezable_power_efficient_wq = alloc_workqueue("events_freezable_power_efficient",WQ_FREEZABLE | WQ_POWER_EFFICIENT,0);
```

不同的 flag 代表工作队列不同的属性，比如 ：
0 表示普通的工作队列，system_wq 就是普通类型，不知道你还记不记得，开发者调用 schedule_work(work) 时，默认将 work 加入到当前工作队列中。  
* WQ_HIGHPRI           表示高优先级的工作队列
* WQ_UNBOUND           表示不和 cpu 绑定的工作队列
* WQ_FREEZABLE         表示可在挂起的时候，该工作队列也会被冻结
* WQ_POWER_EFFICIENT   表示节能型的工作队列
* WQ_MEM_RECLAIM       表示可在内存回收时调用
* WQ_CPU_INTENSIVE     表示 cpu 消耗型
* WQ_SYSFS             表示将当前工作队列导出到 sysfs 中

如果你的 work 有特殊的需求，除了调用 schedule_work()，同样可以调用 queue_work() 以指定将 work 加入到上述的某个工作队列中。  

###　alloc_workqueue　实现
紧接着，我们来看看　alloc_workqueue　的源码实现

```c++
#define alloc_workqueue(fmt, flags, max_active, args...)		\
	__alloc_workqueue_key((fmt), (flags), (max_active),		\
			      NULL, NULL, ##args)               
```
alloc_workqueue 调用了 __alloc_workqueue_key：

```c++
struct workqueue_struct *__alloc_workqueue_key(const char *fmt,unsigned int flags,
					       int max_active,struct lock_class_key *key,const char *lock_name, ...)
{
    struct workqueue_struct *wq;
	struct pool_workqueue *pwq;
    ...
    //申请一个 workqueue_struct 结构。
    wq = kzalloc(sizeof(*wq) + tbl_size, GFP_KERNEL);  

    //如果是 unbound 类型的，就申请默认的 attrs，如果不是，就会使用 percpu worker_pool 的 attrs？？？。
    if (flags & WQ_UNBOUND) {
		wq->unbound_attrs = alloc_workqueue_attrs(GFP_KERNEL);
	}

    //初始化 wq 的成员
    wq->flags = flags;
	wq->saved_max_active = max_active;
	mutex_init(&wq->mutex);
	atomic_set(&wq->nr_pwqs_to_flush, 0);
	INIT_LIST_HEAD(&wq->pwqs);
	INIT_LIST_HEAD(&wq->flusher_queue);
	INIT_LIST_HEAD(&wq->flusher_overflow);
	INIT_LIST_HEAD(&wq->maydays);
    INIT_LIST_HEAD(&wq->list);

    //申请并绑定 pwq 和 wq
    alloc_and_link_pwqs(wq);

    //如果设置了 WQ_MEM_RECLAIM 标志位，需要创建一个 rescue 线程。  
    if (flags & WQ_MEM_RECLAIM) {
		struct worker *rescuer;

		rescuer = alloc_worker(NUMA_NO_NODE);
		if (!rescuer)
			goto err_destroy;

		rescuer->rescue_wq = wq;
		rescuer->task = kthread_create(rescuer_thread, rescuer, "%s",
					       wq->name);
		if (IS_ERR(rescuer->task)) {
			kfree(rescuer);
			goto err_destroy;
		}

		wq->rescuer = rescuer;
		kthread_bind_mask(rescuer->task, cpu_possible_mask);
		wake_up_process(rescuer->task);
	}

    //如果设置了 WQ_SYSFS，将当前 wq 导出到 sysfs 中。
    if ((wq->flags & WQ_SYSFS) && workqueue_sysfs_register(wq))
        goto err_destroy;
    
    //将当前 wq 添加到全局 wq 链表 workqueues 中。
    list_add_tail_rcu(&wq->list, &workqueues);
}
```

从以上源码中可以看到，alloc_workqueue 主要包括几个部分：
* unbound 类型的 wq 需要做的一些额外设置，因为是 unbound 的类型，所以不能使用特定 cpu 的参数。
* wq 主要成员的初始化
* 申请 pwq 并将其和 wq 绑定。
* 如果设置了 WQ_MEM_RECLAIM 标志，需要多创建一个 rescuer 线程。
* 如果设置了 WQ_SYSFS 标志，导出到 sysfs
* 将 wq 链接到全局链表中。  

其中需要讲解的有两部分：
* rescue 线程
* 申请 pwq 并绑定。  

## rescue 线程
在上一章结构体的介绍中，经常可以看到 mayday 和 rescue 的身影，从字面意思来看，这是用来"救命"的东西，至于救谁的命呢？  

结合 WQ_MEM_RECLAIM 标志以及它的注释可以了解到，当 linux 进行内存回收时，可能导致工作队列运行得没那么顺利，这时候需要有一个线程来保证 work 的持续运行，但是并不建议开发者无理由地添加这个 flag，因为这会实实在在地创建一个内核线程，正如前面的章节所说，过多的内核线程将会影响系统的执行效率。  

但是实际上内核中大部分驱动程序的实现都使用了这个标志位。  


## 将 wq 链接到全局链表中
这一部分由函数 ： alloc_and_link_pwqs 完成，


```c++
static int alloc_and_link_pwqs(struct workqueue_struct *wq)
{
	bool highpri = wq->flags & WQ_HIGHPRI;
	int cpu, ret;

	if (!(wq->flags & WQ_UNBOUND)) {
		wq->cpu_pwqs = alloc_percpu(struct pool_workqueue);
		if (!wq->cpu_pwqs)
			return -ENOMEM;

		for_each_possible_cpu(cpu) {
			struct pool_workqueue *pwq =
				per_cpu_ptr(wq->cpu_pwqs, cpu);
			struct worker_pool *cpu_pools =
				per_cpu(cpu_worker_pools, cpu);

			init_pwq(pwq, wq, &cpu_pools[highpri]);

			mutex_lock(&wq->mutex);
			link_pwq(pwq);
			mutex_unlock(&wq->mutex);
		}
		return 0;
	} else if (wq->flags & __WQ_ORDERED) {
		ret = apply_workqueue_attrs(wq, ordered_wq_attrs[highpri]);
		/* there should only be single pwq for ordering guarantee */
		WARN(!ret && (wq->pwqs.next != &wq->dfl_pwq->pwqs_node ||
			      wq->pwqs.prev != &wq->dfl_pwq->pwqs_node),
		     "ordering guarantee broken for workqueue %s\n", wq->name);
		return ret;
	} else {
		return apply_workqueue_attrs(wq, unbound_std_wq_attrs[highpri]);
	}
}
```




