# linux 工作队列 - schedule_work 的实现
了解了 workqueue 的初始化以及实现原理，再来看 schedule_work 的实现原理，事情就会变得轻松很多，经过前面章节的铺垫，不难猜到：不管是 schedule_work 还是 schedule_delayed_work，都是将 work 添加到 worker_pool->worklist 中，然后由 worker 对应的内核线程执行，但是还有两个问题需要给出答案：
* 如何判断将当前的 work 添加到哪个 worker_pool 中？ 
* 这个添加的过程中，pwq 和 workqueue_struct、worker_pool 是如何进行交互的？

带着这两个疑问，我们来看看它的源码实现。  

## schedule_work 
在 workqueue 的使用篇就讲到，schedule_work 会将 work 添加到默认的工作队列也就是 system_wq 中，如果需要添加到指定的工作队列，可以调用 queue_work(wq,work) 接口，第一个参数就是指定的 workqueue_struct 结构。   

schedule_work 其实就是基于 queue_work 的一层封装：

```
static inline bool schedule_work(struct work_struct *work)
{
	return queue_work(system_wq, work);
}
```

接着查看 queue_work 的实现：

```
static inline bool queue_work(struct workqueue_struct *wq,
			      struct work_struct *work)
{
	return queue_work_on(WORK_CPU_UNBOUND, wq, work);
}
```

继续调用 queue_work_on ，增加一个参数 WORK_CPU_UNBOUND，这个参数并不是指将当前 work 绑定到 unbound 类型的 worker_pool 中，只是说明调用者并不指定将当前 work 绑定到哪个 cpu 上，由系统来分配 cpu.当然，调用者也可以直接使用 queue_work_on 接口，通过第一个参数来指定当前 work 绑定的 cpu。  

接着看 queue_work_on 的源码实现：

```c++
bool queue_work_on(int cpu, struct workqueue_struct *wq,
		   struct work_struct *work)
{
    ...
	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
		__queue_work(cpu, wq, work);
		ret = true;
	}
    ...
	return ret;
}
```

如果没有为当前的 work 设置 WORK_STRUCT_PENDING_BIT 标志位，则继续调用 __queue_work 函数：

```c++
static void __queue_work(int cpu, struct workqueue_struct *wq,
			 struct work_struct *work)
{
    ...
    //获取 cpu 相关参数
    if (req_cpu == WORK_CPU_UNBOUND)
		cpu = wq_select_unbound_cpu(raw_smp_processor_id());
    ...

    //检查当前的 work 是不是在这之前被添加到其他 worker_pool 中，如果是，就让它继续在原本的 worker_pool 上运行
    last_pool = get_work_pool(work);
	if (last_pool && last_pool != pwq->pool) {
		struct worker *worker;

		spin_lock(&last_pool->lock);

		worker = find_worker_executing_work(last_pool, work);

		if (worker && worker->current_pwq->wq == wq) {
			pwq = worker->current_pwq;
		} else {
			/* meh... not running there, queue here */
			spin_unlock(&last_pool->lock);
			spin_lock(&pwq->pool->lock);
		}
	} else {
		spin_lock(&pwq->pool->lock);
	}

    //如果超过 pwq 支持的最大的 work 数量，将work添加到 pwq->delayed_works 中，否则就添加到 pwq->pool->worklist 中。  
    if (likely(pwq->nr_active < pwq->max_active)) {
		trace_workqueue_activate_work(work);
		pwq->nr_active++;
		worklist = &pwq->pool->worklist;
		if (list_empty(worklist))
			pwq->pool->watchdog_ts = jiffies;
	} else {
		work_flags |= WORK_STRUCT_DELAYED;
		worklist = &pwq->delayed_works;
	}
    //添加 work 到队列中。
	insert_work(pwq, work, worklist, work_flags);
}
```
__queue_work 主要由几个部分组成：
* 获取 cpu 参数
* 检查冲突
* 添加 work 到队列。

### 获取 cpu 参数
回顾开篇所提出的第一个问题：如何判断将当前的 work 添加到哪个 worker_pool 中？  

在这个函数中就可以得到答案：通过 raw_smp_processor_id() 函数获取当前 cpu 的 id，通过 per_cpu_ptr(pwq，cpu) 接口获取当前 cpu 的 pwq，因为 pwq 向上连接了 workqueue_struct，向下连接了 worker_pool，所以因此也可以获取到当前 cpu 的 worker_pool。  

如果指定添加到 unbound 类型的 workqueue_struct 上,就使用 unbound 类型的 pwq 和 worker_pool。  

在特殊情况下，当前的 cpu 上没有初始化 worker_pool 和 pwq，就会找到下一个可用的 cpu。 

### 检查冲突
检查当前的 work 是不是在这之前被添加到其他 worker_pool 中，如果是，就让它继续在原本的 worker_pool 上运行，这时候找到的 pwq 指向另一个 cpu 的 pwq 结构。   

内核这样设计的初衷应该是出于 cpu 高速缓存的考虑，某个 cpu 曾经执行过该 work，所以将该 work 放到之前的 cpu 上执行可能因为缓存命中而提高执行效率，但是这也只是可能。除非两次 work 执行间隔非常小，高速缓存才有可能会保留。   

### 添加 work 到队列
将 work 添加到队列的函数是 insert_work(pwq, work, worklist, work_flags)，传入的参数中有 work 和 worklist，如果超过 pwq 支持的最大的 work 数量，将work添加到 pwq->delayed_works 中，否则就添加到 pwq->pool->worklist 中。   

insert_work 的源码实现也比较简单：

```c++
static void insert_work(struct pool_workqueue *pwq, struct work_struct *work,
			struct list_head *head, unsigned int extra_flags)
{
	struct worker_pool *pool = pwq->pool;
    //设置 work 的 pwq 和 flag。
	set_work_pwq(work, pwq, extra_flags);
    //将 work 添加到 worklist 链表中
	list_add_tail(&work->entry, head);
    //为 pwq 添加引用计数
	get_pwq(pwq);
    //添加内存屏障，防止 cpu 将指令乱序排列
	smp_mb();
    
    //唤醒 worker 对应的内核线程
	if (__need_more_worker(pool))
		wake_up_worker(pool);
}
```
简单地说，就是将 work 插入到 worker_pool->worklist 中。   


尽管在前面的章节中有说明，但是博主觉得在这里有必要再强调一次：每一个通过 alloc_workqueue 创建的 wq 都会对应 percpu 的 pwq 和 percpu 的 worker_pool，比如在一个四核系统中，一个 alloc_workqueue 对应 4 个 pwq 和 4 个 worker_pool。   

将 work 添加到 wq 的哪个 worker 呢？ 就是执行当前 schedule_work 代码那个，添加完之后，就会唤醒 worker_pool 中第一个处于 idle 状态 worker->task 内核线程，work 就会进入到待处理状态，至于如何处理的，可以参考上一章内核线程的执行。  

对于开篇的第二个问题：这个添加的过程中，pwq 和 workqueue_struct、worker_pool 是如何进行交互的？   

答案其实也很简单，获取当前 cpu 的 id，通过该 cpu id 获取当前被使用的 wq 绑定的 pwq，通过 pwq 就可以找到对应的 worker_pool，在这里 pwq 相当于 worker_pool 和 wq 之间的媒介。  









