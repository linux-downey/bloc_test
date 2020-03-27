# linux 下半部机制-tasklet 的实现原理

按照惯例，对于 linux 的接口，我们的讨论都是由表及里，先从内核提供的 API 开始，先知道如何使用，然后再进一步深入原理，这一章我们来讨论 tasklet 的实现原理。  

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

内核创建了两个 percpu 类型的 tasklet 头结点，一个对应高优先级的 tasklet，一个对应 普通优先级 tasklet，系统就是通过头结点来对 tasklet 进行管理，当用户需要使用 tasklet 函数时，就会把用户的 tasklet 结构链接到该头结点上，在执行 tasklet 时，就从头结点开始逐个地取出 tasklet 成员，然后执行。  



### 软中断的初始化
tasklet 是基于软中断实现的，这是我们上一章就提到的，在软中断的枚举列表中，有这么两项：

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






