# 等待队列源码分析
在 Linux 平台下，分析接口如果仅仅是根据文档写两个demo，完全没有深刻的理解，过两天就忘，这样的学习效率非常低下。  

我们要做的就是通过阅读它的源代码实现来弄清楚它的来龙去脉，掌握它的核心框架。正如博主常说的那句话，优秀的软件开发工程师应该永远保持着对软件实现原理的好奇心，看到一个新事物，应该多想想它是如何实现的。    


## wait_event（wq_head，condition）
wait_event 是最基本的接口，将一个进程加入到等待队列中，它的源码实现是这样的：

```
#define wait_event(wq_head, condition)						\
do {										\
	might_sleep();								\
	if (condition)								\
		break;								\
	__wait_event(wq_head, condition);					\
} while (0)
```

这是一个宏定义，传入两个参数，第一个是等待队列头，第二个是等待条件。  

might_sleep() 这个函数事实上什么也不做，它只是告诉你，这个接口可能导致睡眠，不要在不支持睡眠的上下文中调用。  

如果 condition 为真，则直接跳出该函数的执行并返回，也就是说，当用户传入的 condition 为真时，该进程并不会进入睡眠，而是直接往下执行代码。  

__wait_event的实现是这样的：
```
#define __wait_event(wq_head, condition)					\
	(void)___wait_event(wq_head, condition, TASK_UNINTERRUPTIBLE, 0, 0,	\
			    schedule())
```
它直接调用 ___wait_event 函数，值得注意的是，调用者的开头是两个下划线，而被调用者是三个，容易混淆。___wait_event 接口就是处理进程睡眠的主要实现函数了，这个最核心的函数放到最后统一讲解。


## wait_event_interruptible(wq_head, condition)
与 wait_event 不一样的是，wait_event_interruptible 在睡眠过程中会响应信号，打个比方：当你在用户空间读一个文件时，因资源不足而在等待队列上睡眠等待时，wait_event_interruptible 方式的睡眠可以被CTRL+C 终止，而 wait_event 形式的睡眠时，当你按下 CTRL+C，不会有任何反应。    

```
#define wait_event_interruptible(wq_head, condition)				\
({										\
	int __ret = 0;								\
	might_sleep();								\
	if (!(condition))							\
		__ret = __wait_event_interruptible(wq_head, condition);		\
	__ret;									\
})
```
同样的，调用 might_sleep 和判断 condition，不再赘述。  

然后调用 __wait_event_interruptible 接口：

```
#define __wait_event_interruptible(wq_head, condition)				\
	___wait_event(wq_head, condition, TASK_INTERRUPTIBLE, 0, 0,		\
		      schedule())
```
__wait_event_interruptible 接口同样是调用了 ___wait_event，与 wait_event 不同的是：wait_event 传入的进程状态是 TASK_UNINTERRUPTIBLE，而 wait_event_interruptible 传入的是 TASK_INTERRUPTIBLE，接下来就是 ___wait_event 函数的处理，同样放到后面统一讲解。  


## wait_event_timeout()
wait_event_timeout 接口允许设置一个超时事件，如果等待超过超时时间，尽管 condition 仍然为 false，也唤醒进程。  

```
#define wait_event_timeout(wq_head, condition, timeout)				\
({										\
	long __ret = timeout;							\
	might_sleep();								\
	if (!___wait_cond_timeout(condition))					\
		__ret = __wait_event_timeout(wq_head, condition, timeout);	\
	__ret;									\
})
```
该宏的实现多了一个参数：timeout，timeout 以 jiffies 为单位。  

首先判断 ___wait_cond_timeout(condition)，当它返回 0 时，才会进入睡眠处理函数，它的实现是这样的：
```
#define ___wait_cond_timeout(condition)						\
({										\
	bool __cond = (condition);						\
	if (__cond && !__ret)							\
		__ret = 1;							\
	__cond || !__ret;							\
})
```
在 wait_event_timeout 的宏实现中，__ret 被赋值为用户设置的 timeout(注意：宏只是简单的替换，所以相当于是继承了上一个宏的变量)，如果 condition 为 true 或者 __ret(即 timeout)为 0，返回 true，将会退出 wait_event_timeout 接口，简言之就是不进入睡眠。  

如果 ___wait_cond_timeout 返回 0，就按正常流程调用 __wait_event_timeout：

```C
#define __wait_event_timeout(wq_head, condition, timeout)			\
	___wait_event(wq_head, ___wait_cond_timeout(condition),			\
		      TASK_UNINTERRUPTIBLE, 0, timeout,				\
		      __ret = schedule_timeout(__ret))
```
同样，这个接口也是调用了 ___wait_event 接口，其中 ___wait_cond_timeout(condition) 为 0，所以它与另外两个接口的区别是：
* 设置的进程状态为 TASK_UNINTERRUPTIBLE，表示不可接收信号
* 传入了 timeout 参数，而其他两个是 0
* 最后一个参数执行的是  schedule_timeout(__ret)，而另外两个接口是执行 schedule()，从名称就可以看出区别。  


## ___wait_event

不难发现，wait_event、wait_event_interruptible、wait_event_timeout 三个接口都是调用了 ___wait_event 接口，只是传入的参数不一样，我们就来看看这个接口的实现。

```
#define ___wait_event(wq_head, condition, state, exclusive, ret, cmd)		\
({										\
	__label__ __out;							\
	struct wait_queue_entry __wq_entry;					\
	long __ret = ret;	        				\
										\
	init_wait_entry(&__wq_entry, exclusive ? WQ_FLAG_EXCLUSIVE : 0);	\
	for (;;) {								\
		long __int = prepare_to_wait_event(&wq_head, &__wq_entry, state);\
										\
		if (condition)							\
			break;							\
										\
		if (___wait_is_interruptible(state) && __int) {			\
			__ret = __int;						\
			goto __out;						\
		}								\
										\
		cmd;								\
	}									\
	finish_wait(&wq_head, &__wq_entry);					\
__out:	__ret;									\
})
```

这个接口同样是以宏的方式实现的，它的实现总共包含三个部分：
* 由 init_wait_entry/finish_wait 两个接口对接入链表的 entry 进行管理，分别负责初始化和注销。
* prepare_to_wait_event：进入睡眠的准备工作
* 执行真正的睡眠函数：cmd，这是传入的参数，对于 wait_event 就是简单的 schedule(),对于 wait_event_timeout 执行的是 schedule_timeout(timeout)   


###　entry 的初始化
在前文中有提到：等待队列的组织形式为链表，而我们整个对于等待队列的操作都只涉及到等待队列头，事实上每个睡眠在等待队列上的进程都有对应的节点结构体，只是这个节点被系统的接口隐藏了。  

init_wait_entry 用于初始化一个节点：

```
struct wait_queue_entry {
	unsigned int		flags;
	void			*private;
	wait_queue_func_t	func;
	struct list_head	entry;
};

void init_wait_entry(struct wait_queue_entry *wq_entry, int flags)
{
	wq_entry->flags = flags;
	wq_entry->private = current;
	wq_entry->func = autoremove_wake_function;
	INIT_LIST_HEAD(&wq_entry->entry);
}
```

该函数就是初始化等待队列节点的四个成员：

flags 通常为0，表示不指定特定进程的操作。

值得特别注意的是 private 和 func 这两个成员， private 被赋值为当前进程的 task_struct 指针，在唤醒或者睡眠的时候需要设置进程状态，这时候需要这个指针。

而 func 被赋值为 autoremove_wake_function，这个函数是系统提供的一个唤醒实现函数，它负责唤醒进程，当然，这是个回调函数，在后续适当的时候会被调用。  
 
整个 wait_event 操作被包含在一个死循环中，也就是说，即使当前进程被唤醒，如果 condition 部位 true，又会循环进入睡眠，当进程被唤醒同时 condition 为真时，才会跳出循环。  

就执行到 finish_wait，从名字可以看出，这个接口主要负责执行收尾工作：

```
unsigned long flags;

	__set_current_state(TASK_RUNNING);
	
	if (!list_empty_careful(&wq_entry->entry)) {
		spin_lock_irqsave(&wq_head->lock, flags);
		list_del_init(&wq_entry->entry);
		spin_unlock_irqrestore(&wq_head->lock, flags);
	}
```
它的实现比较清晰，就是设置当前进程状态为就绪态(TASK_RUNNING)，将该节点从等待队列中删除。  

### 睡眠的准备阶段
睡眠的准备阶段由 prepare_to_wait_event 接口实现：

```C
long prepare_to_wait_event(struct wait_queue_head *wq_head, struct wait_queue_entry *wq_entry, int state)
{
	unsigned long flags;
	long ret = 0;

	spin_lock_irqsave(&wq_head->lock, flags);
	if (unlikely(signal_pending_state(state, current))) {
		list_del_init(&wq_entry->entry);
		ret = -ERESTARTSYS;
	} else {
		if (list_empty(&wq_entry->entry)) {
			if (wq_entry->flags & WQ_FLAG_EXCLUSIVE)
				__add_wait_queue_entry_tail(wq_head, wq_entry);
			else
				__add_wait_queue(wq_head, wq_entry);
		}
		set_current_state(state);
	}
	spin_unlock_irqrestore(&wq_head->lock, flags);

	return ret;
}
```
signal_pending_state 函数负责检查当前进程是否有信号需要处理，如果有信号就返回真，这种情况下会删除等待队列节点， prepare_to_wait_event 函数返回 -ERESTARTSYS，继续执行下面的代码：

```
if (___wait_is_interruptible(state) && __int) {			\
			__ret = __int;						\
			goto __out;						\
		}
__out:	__ret;
```
___wait_is_interruptible 接口主要是检测用户传入的 state 是否为 TASK_INTERRUPTIBLE 或者 TASK_KILLABLE，如果是，返回真，如果同时 prepare_to_wait_event 返回值不为0(表示收到信号)，就跳出循环，进程就得以继续向下运行，就是我们通常理解的唤醒。  

上文中有提到 wait_event_interruptible 接口就是传入的 TASK_INTERRUPTIBLE 进程状态。  









