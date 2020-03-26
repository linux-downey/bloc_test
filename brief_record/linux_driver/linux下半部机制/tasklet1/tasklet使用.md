# linux 下半部机制-tasklet的使用

在 linux 的下半部机制中，tasklet 算是一个折中的方案，相对于 softirq 而言，它算是一个"简单好用"的接口，简单好用主要在于两点：

* tasklet 不允许同一个 tasklet 在多个 CPU 上并发执行，同时 tasklet 是 percpu 实现的，所以不需要考虑 SMP 下的并发操作，因此其执行函数在编程上要简单很多，要知道处理并发向来是个麻烦的问题
* tasklet 可以动态地定义，实现更方便

当然，相比于 softirq 而言，由于不支持多 CPU 上的并发执行，所以在执行触发频率较高的 task 时，执行效率要低一些，这体现在同一个 softirq 可以同时被添加到不同 CPU 上，而同一个 tasklet，在当前 tasklet 没有被执行前，是不能添加到 tasklet 队列中的，即使是不同的 CPU。   

而且，当一个 tasklet 在执行时，同一个 tasklet 即使被添加到 tasklet 队列中，也需要等到第一个执行完才能继续执行。  

而对于 workqueue 而言，tasklet 执行效率自然是要高一些的，因为 tasklet 的实现基于 softirq，不过同样还是运行在中断上下文，执行函数中不能出现任何可能导致阻塞的代码，这和 workqueue 是本质上的区别。  


## tasklet 的使用
和其他的下半部机制一样，tasklet 的使用依旧分为两个部分：

* 初始化
* 调度使用


## tasklet 结构

要了解 tasklet 结构的使用，就得先了解描述一个 tasklet 的数据结构是怎么样的，一个 tasklet 结构由 struct tasklet_struct 来描述：

```c++
struct tasklet_struct
{
	struct tasklet_struct *next;  //指向下一个 tasklet 成员
	unsigned long state;          //tasklet 的运行状态
	atomic_t count;               //tasklet 是否使能
	void (*func)(unsigned long);  //执行函数
	unsigned long data;           //执行函数的参数
};
```
tasklet 结构并不复杂，基本上是一目了然：  

cpu 的 tasklet 队列是以链表的形式对 tasklet 进行管理的，所以需要一个 next 指针，指向下一个添加到待执行链表中 tasklet 成员。  

state 中的标志位分为两种：TASKLET_STATE_SCHED 和 TASKLET_STATE_RUN

对于 TASKLET_STATE_SCHED，该位被置位表示该 tasklet 已经被添加到某个 CPU 上的 tasklet 队列中，但是还没有被执行，对于已经添加的 tasklet，不允许重复添加到其他或本地 CPU 的 tasklet 待运行队列中。  

另一个 state：TASKLET_STATE_RUN，这个只有在 SMP 架构中才会被使用，CPU 在执行 tasklet 时要检查该标志位，如果该标志位被置位，表示该 tasklet 正在某个 CPU 上被执行，则当前 tasklet 不允许执行，直到该 tasklet 成员被执行完，也就是等到该标志位被置为0.  

count 则表示 tasklet 是否使能，0 表示 enable，大于 0 表示 disable，至于为什么使用 count 而不是使用 bool 类型的值，是因为 enable 和 disable 支持嵌套地执行，当该 tasklet 被 disable 两次时，count 就为 2，同样需要调用两次 enable 才能使能该 tasklet。  


## tasklet 结构的初始化

初始化一个 tasklet 结构，有两种方式，一种是静态的定义，一种是动态的定义：

```c++
#define DECLARE_TASKLET(name, func, data) \
struct tasklet_struct name = { NULL, 0, ATOMIC_INIT(0), func, data }

#define DECLARE_TASKLET_DISABLED(name, func, data) \
struct tasklet_struct name = { NULL, 0, ATOMIC_INIT(1), func, data }
```

这两个定义唯一的不同在于所定义的 tasklet 的初始状态，第二种接口默认定义一个 disable 的 tasklet。  

动态定义的接口为：

```c++
void tasklet_init(struct tasklet_struct *t,
		  void (*func)(unsigned long), unsigned long data)
{
	t->next = NULL;
	t->state = 0;
	atomic_set(&t->count, 0);
	t->func = func;
	t->data = data;
}
```
这没啥好说的，就是对成员进行初始化和赋值。  



## tasklet 的调度
tasklet 初始化完成之后调度执行，需要使用下面的接口：

```
void tasklet_schedule(struct tasklet_struct *t);
void tasklet_hi_schedule(struct tasklet_struct *t)
```
这两个接口的调用都是传入已经初始化完成的 tasklet 指针，第一个接口是默认的、常用的接口，表示将该 tasklet 添加到当前 cpu 默认的 tasklet 队列中，而第二个接口则是将 tasklet 添加到高优先级的 tasklet 队列中。   

调度完成之后， tasklet 就进入到待执行状态，将会在软中断被调度时被执行。  






















