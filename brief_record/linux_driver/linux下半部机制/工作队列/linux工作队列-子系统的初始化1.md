# linux 工作队列-子系统的初始化第二阶段
通过上一章的讨论，我们了解了工作队列初始化的第一阶段创建了 worker_pool 和系统默认的 workqueue，在初始化的第二阶段做了哪些事呢？    

我们继续来看另一个初始化函数：workqueue_init(),我们先看一下这个函数的总体实现，然后再进行局部分析：

```
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

### 确定内核线程名称
在 worker_pool 的创建时，使用 worker_pool_assign_id 接口为每个 worker_pool 分配了唯一 id 号，这个 id 号在这里被用于内核线程名称的确定。   

对于系统默认创建的工作队列线程，我们可以在系统终端使用下面的指令来查看：

```
ps -ef | grep "*worker*"
```











