# linux 中断子系统 - 工作队列的初始化1
通过上一章的讨论，我们了解了工作队列初始化的第一阶段创建了 worker_pool 和系统默认的 workqueue，在初始化的第二阶段做了哪些事呢？    

我们继续来看另一个初始化函数：workqueue_init()，我们先看一下这个函数的总体实现，然后再进行局部分析：

```c++
int __init workqueue_init(void)
{
	struct workqueue_struct *wq;
	struct worker_pool *pool;
	int cpu, bkt;

    // NUMA 节点的初始化
	wq_numa_init();

    //设置 NUMA node
	for_each_possible_cpu(cpu) {
		for_each_cpu_worker_pool(pool, cpu) {
			pool->node = cpu_to_node(cpu);
		}
	}

	list_for_each_entry(wq, &workqueues, list)
		wq_update_unbound_numa(wq, smp_processor_id(), true);

    //对每个 CPU 的 worker_pool ，创建 worker 线程
	for_each_online_cpu(cpu) {
		for_each_cpu_worker_pool(pool, cpu) {
			pool->flags &= ~POOL_DISASSOCIATED;
			BUG_ON(!create_worker(pool));
		}
	}

    //创建 unbound 线程
	hash_for_each(unbound_pool_hash, bkt, pool, hash_node)
		BUG_ON(!create_worker(pool));

    ...
	return 0;
}

```

第二阶段的初始化分为以下的几个主要部分：



* numa 的初始化
* 为每个 cpu 和 unbound 类型的 worker_pool ，创建默认的 worker 线程




### numa 的初始化
在初始化的第一阶段中，就涉及到了 NUMA 节点的处理，事实上，在初始化的第一阶段就做这件事是比较简单且合适的，但是对于某些架构而言，比如 power 和 arm64，NUMA 节点映射在初始化的第一阶段之后，所以在第二阶段依旧需要对 numa 节点进行初始化以及更新，主要是针对每个 worker_pool 和 unbound 类型的 wq。  

numa 架构相关的不过多讨论。



### 为 worker_pool 创建 worker 线程
通过系统提供的接口：create_worker(pool) 为每一个 worker_pool 构建 worker 并创建内核线程，传入的参数为 worker_pool.  

接下来自然要看看 create_worker 的实现：

```c++
static struct worker *create_worker(struct worker_pool *pool)
{
	struct worker *worker = NULL;
	int id = -1;
	char id_buf[16];

    // id 用作确定内核线程的名称
	id = ida_simple_get(&pool->worker_ida, 0, 0, GFP_KERNEL);
	
    //申请一个 worker 结构
	worker = alloc_worker(pool->node);
    //
	worker->pool = pool;
	worker->id = id;

    //确定内核线程的名称
	if (pool->cpu >= 0)
		snprintf(id_buf, sizeof(id_buf), "%d:%d%s", pool->cpu, id,
			 pool->attrs->nice < 0  ? "H" : "");
	else
		snprintf(id_buf, sizeof(id_buf), "u%d:%d", pool->id, id);

    //创建内核线程
	worker->task = kthread_create_on_node(worker_thread, worker, pool->node,
					      "kworker/%s", id_buf);

	set_user_nice(worker->task, pool->attrs->nice);
	kthread_bind_mask(worker->task, pool->attrs->cpumask);

	worker_attach_to_pool(worker, pool);

	worker->pool->nr_workers++;
	worker_enter_idle(worker);
    //开始执行内核线程
	wake_up_process(worker->task);

	return worker;
}
```
create_worker 分成以下的几个步骤：



* 申请一个 worker 结构
* 确定内核线程名称
* 创建内核线程



### 申请 worker 结构

worker 结构体用于描述 workqueue 相关的内核线程，需要使用时要向系统申请并初始化一个 worker 结构，该操作由 alloc_worker 这个接口完成，参数 node 表示 NUMA node，如果是在 NUMA 系统上，则会在 pool->node 节点上进行申请，然后进行一些成员的初始化。    

### 确定内核线程名称并绑定
在 worker_pool 的创建时，使用 worker_pool_assign_id 接口为每个 worker_pool 分配了唯一 id 号，这个 id 号在这里被用于内核线程名称的确定。   

在使用 linux 系统时，当我们使用 ps 命令进程，经常可以看到类似下面的这种条目：

```
root      5715     2  0 Mar12 ?        00:01:28 [kworker/2:2]
...
```
进程的名称为 kworker/%d:%d ,通常会有多个，这些就是内核默认的工作队列所产生的线程，而这些线程的就是在通过上述 create_worker 接口创建的。   


我们可以在系统终端使用下面的指令来指定查看系统中正在运行的工作线程：

```
ps -ef | grep "kworker*"
```

系统的输出通常是这样的：

```
...
root      9099     2  0 Mar15 ?        00:01:00 [kworker/1:2]
root      9679     2  0 Feb24 ?        00:02:27 [kworker/0:1]
root     21980     2  0 08:20 ?        00:00:00 [kworker/0:0H]
root     22033     2  0 08:27 ?        00:00:00 [kworker/1:0H]
root     22127     2  0 08:44 ?        00:00:02 [kworker/u8:1]
...
```

其中：

"/" 后的第一个字节表示 cpu id，而 u 则代表 unbound 类型的 worker，":"后的数字则表示该线程对应的 worker_pool 的 id,后缀为 "H" 表示为高优先级 worker_pool 的 worker。  




### 创建内核线程并绑定

创建内核线程和绑定主要是下面这两个接口：

```
worker->task = kthread_create_on_node(worker_thread, worker, pool->node,"kworker/%s", id_buf);
worker_attach_to_pool(worker, pool);
```
使用 kthread_create_on_node 接口创建内核线程以兼容 NUMA 系统，将返回的 task_struct 赋值给 worker，由 worker 对线程进行管理。worker_attach_to_pool 接口会将 worker 绑定到 worker_pool 上，其实就是执行下面这一段代码：

```
list_add_tail(&worker->node, &pool->workers);
```
通过 worker->node 节点链接到 worker_pool 的 workers 链表上。  

当前 thread 的 thread_func 为 worker_thread ，参数为当前 worker，这是内核线程的执行主体部分，同样的，我们来看看它的源码实现，看看工作线程到底做了哪些事：

```c++
static int worker_thread(void *__worker)
{
	struct worker *worker = __worker;
	struct worker_pool *pool = worker->pool;

	
	worker->task->flags |= PF_WQ_WORKER;
woke_up:
	spin_lock_irq(&pool->lock);
	//在必要的时候删除 worker，退出当前线程。
	if (unlikely(worker->flags & WORKER_DIE)) {
		spin_unlock_irq(&pool->lock);
		WARN_ON_ONCE(!list_empty(&worker->entry));
		worker->task->flags &= ~PF_WQ_WORKER;

		set_task_comm(worker->task, "kworker/dying");
		ida_simple_remove(&pool->worker_ida, worker->id);
		worker_detach_from_pool(worker, pool);
		kfree(worker);
		return 0;
	}

	worker_leave_idle(worker);
recheck:
	//管理 worker 线程
	if (!need_more_worker(pool))
		goto sleep;

	if (unlikely(!may_start_working(pool)) && manage_workers(worker))
		goto recheck;

	//执行 work
	do {
		struct work_struct *work =
			list_first_entry(&pool->worklist,
					 struct work_struct, entry);

		pool->watchdog_ts = jiffies;

		if (likely(!(*work_data_bits(work) & WORK_STRUCT_LINKED))) {
			process_one_work(worker, work);
			if (unlikely(!list_empty(&worker->scheduled)))
				process_scheduled_works(worker);
		} else {
			move_linked_works(work, &worker->scheduled, NULL);
			process_scheduled_works(worker);
		}
	} while (keep_working(pool));

	worker_set_flags(worker, WORKER_PREP);
sleep:
	//处理完成，陷入睡眠
	worker_enter_idle(worker);
	__set_current_state(TASK_IDLE);
	spin_unlock_irq(&pool->lock);
	schedule();
	goto woke_up;
}
```
整个 worker_thread 函数实现比较多，这里同样只是截取了主体部分，该函数主要包括以下的几个主要部分：



* 管理 worker
* 执行 work



#### 管理 worker 
从 worker 中获取与其绑定的 worker_pool，在 worker_pool 的基础上对运行在其上的 worker 进行管理，主要分为两个部分：检查删除和检查创建。  

检查删除主要是检查 worker->flags & WORKER_DIE 标志位是否被置位，如果该位被置位，将释放当前的 worker ，当前线程返回，回收相应的资源。这个标志位是通过 destroy_worker 函数置位，删除的逻辑就是当前 worker_pool 中有冗余的 worker，长时间进入 idle 状态没有被使用。   

检查创建主要由 manage_workers 实现，当 worker_pool 的负载增高时，需要动态地新创建 worker 来执行 work，每个 pool 至少保证有一个 idle worker 以响应即将到来的 work。  

worker 的创建和删除完全取决于需要执行 work 数量，而这也是整个 work queue 新框架的核心部分。   



#### 执行 work
worker 用于管理线程，而线程自然是用于执行具体的工作，从源码中不难看到，当前 worker 的线程被唤醒后将会从第一个 worker_pool->worklist 中的链表元素，也就是挂入当前 worker_pool 的 work 开始，取出其中的 work，然后执行，执行的接口使用：

```
move_linked_works(work, &worker->scheduled, NULL);
process_scheduled_works(worker);
```
move_linked_works 将会在执行前将 work 添加到 worker->scheduled 链表中，该接口和 list_add_tail 不同的是，这个接口会先删除链表中存在的节点并重新添加，保证不会重复添加，且始终添加到最后一个节点。  

然后调用 process_scheduled_works 函数正式执行 work，该函数会遍历 worker->scheduled 链表，执行每一个 work，执行之前会做一些必要的检查，比如在同一个 cpu 上，一个 worker 不能在多个 worker 线程中被并发执行(这里的并发执行指的是同时加入到 schedule 链表)，是否需要唤醒其它的 worker 来协助执行(碰到 cpu 消耗型的 work 需要这么做)，执行 work 的方式就是调用 work->func，。  

当执行完 worker_pool->worklist 中所有的 work 之后，当前线程就会陷入睡眠。    




## 小结
第二阶段的初始化主要是为每个 worker_pool 创建 worker，并创建对应的内核线程，这些内核线程负责处理 work。  





### 参考

4.14 内核代码

[蜗窝科技：workqueue](http://www.wowotech.net/irq_subsystem/cmwq-intro.html)

[魅族内核团队：workqueue](http://kernel.meizu.com/linux-workqueue.html)

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)









