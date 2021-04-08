# linux 下半部机制-tasklet 的实现原理

按照惯例，对于 linux 的接口，我们的讨论都是由表及里，先从内核提供的 API 开始，先知道如何使用，然后再进一步深入原理，这一章我们来讨论 tasklet 的实现原理，tasklet 的实现在内核文件：kernel/softirq.c 中。  



## tasklet 的实现

### 全局数组
内核中对资源的统一管理通常是通过全局数据结构来实现，tasklet 也不例外，内核默认定义了以下变量来管理 tasklet：

```c++
struct tasklet_head {
	struct tasklet_struct *head;
	struct tasklet_struct **tail;
};

static DEFINE_PER_CPU(struct tasklet_head, tasklet_vec);
static DEFINE_PER_CPU(struct tasklet_head, tasklet_hi_vec);
```

内核创建了两个 percpu 类型的 tasklet 头结点，一个对应高优先级的 tasklet，一个对应普通优先级 tasklet，系统就是通过头结点来对 tasklet 进行管理，当用户需要使用 tasklet 函数时，就会把用户的 tasklet 结构链接到该头结点上，在执行 tasklet 时，就从头结点开始逐个地取出 tasklet 成员，然后执行。  



### 软中断的初始化
tasklet 是基于软中断实现的，这是我们在软中断章节中就提到的，在软中断的枚举列表中，有这么两项：

```c++
enum
{
	HI_SOFTIRQ=0,
	...
	TASKLET_SOFTIRQ,
	...
	NR_SOFTIRQS
};
```
HI_SOFTIRQ 表示高优先级的 softirq，在软中断中，它的执行优先级是最高的，甚至高过内核定时器，而 NR_SOFTIRQS 是普通的 tasklet ，也是内核中默认使用的 tasklet 对应的软中断。   

这两个软中断的初始化在 softirq_init 函数中：

```c++
void __init softirq_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		per_cpu(tasklet_vec, cpu).tail =
			&per_cpu(tasklet_vec, cpu).head;
		per_cpu(tasklet_hi_vec, cpu).tail =
			&per_cpu(tasklet_hi_vec, cpu).head;
	}

	open_softirq(TASKLET_SOFTIRQ, tasklet_action);
	open_softirq(HI_SOFTIRQ, tasklet_hi_action);
}
```

softirq_init 的第一步就是初始化全局列表，将 tail 指针指向 &head，有些朋友对这里的 head 和 tail 感到疑惑，为什么 tail 要使用二级指针。  

一方面，对于 percpu 的操作不能直接通过变量来操作，只能是通过先找到 percpu 变量的地址，然后基于地址来进行修改，所以 tail 需要用到二级指针，同时 tail=&head。  

在插入列表节点 t 时，直接使用：
```
*tail = t;
tail = t->ext;
```
*tail = t 等价于 head = t,也就相当于将节点 t 链接到 head 上，这部分的链表操作确实有点不好理解。  

初始化完全局列表之后就调用 open_softirq 注册 tasklet 对应的软中断，软中断的执行函数为 tasklet_action，tasklet_hi_action 对应高优先级的 tasklet，该执行函数将放到最后分析。  




### tasklet 成员的添加
用户想要添加一个 tasklet 到系统中，需要调用 tasklet_schedule 或者 tasklet_hi_schedule，这里我们只讨论普通 tasklet 的情况，高优先级的 tasklet 和普通的接口几乎一致，没必要重复。  

```
static inline void tasklet_schedule(struct tasklet_struct *t)
{
	if (!test_and_set_bit(TASKLET_STATE_SCHED, &t->state))
		__tasklet_schedule(t);
}
```
在添加该 tasklet 之前，会先判断该 tasklet 是否已经被添加到 tasklet 队列中，即使是其他 cpu 的 tasklet 队列也不行，但是该函数没有返回值，所以即使添加失败了你也不会知道，内核对于 tasklet 只会保证它会执行一次，而不能保证你调用多少次 tasklet_schedule 就执行多少次 tasklet 工作函数，这个需要注意。  

当然，特殊情况下，你也可以直接使用 __tasklet_schedule 接口强制添加到系统中。  

```c++
void __tasklet_schedule(struct tasklet_struct *t)
{
	unsigned long flags;

	local_irq_save(flags);
	t->next = NULL;
	*__this_cpu_read(tasklet_vec.tail) = t;
	__this_cpu_write(tasklet_vec.tail, &(t->next));
	raise_softirq_irqoff(TASKLET_SOFTIRQ);
	local_irq_restore(flags);
}
```
__tasklet_schedule 的实现比较简单，就是将传入的 tasklet 结构链接到 this_cpu.tasklet_vec 链表中。   

然后执行 raise_softirq_irqoff 设置 tasklet 软中断就绪。  



### tasklet 的执行
tasklet 是基于软中断实现的，所以 tasklet 被调度的节点也就是软中断被执行的节点：



* 硬中断退出时执行软中断
* 重新使能软中断的时候执行软中断

而 tasklet 初始化时注册的软中断执行函数为 tasklet_action，tasklet 的处理主要在这个函数中：

```c++
static __latent_entropy void tasklet_action(struct softirq_action *a)
{
	struct tasklet_struct *list;

	ocal_irq_disable();
	//获取链表头结点
	list = __this_cpu_read(tasklet_vec.head);
	//设置全局列表为空，但是 tasklet 节点并没有丢失，依旧可以使用 list 访问
	__this_cpu_write(tasklet_vec.head, NULL);
	__this_cpu_write(tasklet_vec.tail, this_cpu_ptr(&tasklet_vec.head));
	local_irq_enable();

	while (list) {
		struct tasklet_struct *t = list;
		list = list->next;
		if (tasklet_trylock(t)) {
			if (!atomic_read(&t->count)) {
				//清除 TASKLET_STATE_SCHED 标志位
				if (!test_and_clear_bit(TASKLET_STATE_SCHED,
							&t->state))
					BUG();
				//执行 tasklet 的工作函数
				t->func(t->data);
				tasklet_unlock(t);
				continue;
			}
			tasklet_unlock(t);
		}

		local_irq_disable();
		t->next = NULL;
		*__this_cpu_read(tasklet_vec.tail) = t;
		__this_cpu_write(tasklet_vec.tail, &(t->next));
		__raise_softirq_irqoff(TASKLET_SOFTIRQ);
		local_irq_enable();
	}
}
```

tasklet 的执行也非常简单，一旦系统中存在需要调度执行的 tasklet 任务，其对应的软中断就会被执行，然后就是从 tasklet 全局链表的头结点开始，逐个地取出 tasklet 并执行，详细地流程为：



* 执行前获取头结点，并设置链表节点为空，后续使用该头结点来索引所有的 tasklet。 
* 在执行 tasklet 之前需要执行 tasklet_trylock，这个函数在单核系统下直接返回 1，在 SMP 下，这个函数将会设置 state 的 TASKLET_STATE_RUN 位，以限制同一个 tasklet 不会在不同的 cpu 上并发执行。  
* 执行 atomic_read(&t->count)，count 是用来管理 tasklet 的使能状态的计数器，默认为 0 ，表示该 tasklet 是 enable 状态，当大于 0 时，表示为 disable 状态，所以在这里做判断，如果 tasklet->count 大于 0 ，就不执行。  
* 清除 state 的 TASKLET_STATE_SCHED 位。  
* 调用 tasklet 工作函数，然后将设置的 TASKLET_STATE_RUN 清除(单核下什么也不做)。  





### 参考

4.14 内核代码





[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)





