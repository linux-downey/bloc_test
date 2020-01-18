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

事实上，在调用完 ___wait_is_interruptible 之后，如果条件不满足，就会陷入睡眠，但是进程在发送信号时会将目标进程唤醒，所以当该进程被唤醒时，又会从死循环从头执行，从而检测到信号退出睡眠。  


上面说的是信号处理的情况，但是对于 wait_event 、wait_event_timeout 的睡眠方式，或者是 wait_event_interruptible 没有接收到信号的情况下 ，将会直接跳过信号处理的部分，将当前节点链接到等待队列头中，并调用 set_current_state 将进程设置成对应的状态：不可中断睡眠或者可中断睡眠。  

### 进入睡眠
当 ___wait_is_interruptible(state) && __int 条件为假时(需要同时满足可中断睡眠且接收到信号才为真)，执行下一条：cmd。

cmd 是一个宏参数，除了 wait_event_timeout 另两种类型传入的是 schedule(),就是简单的执行切换进程的工作，将由调度器选择合适的进程运行，当前进程在前文的准备睡眠工作中已经被设置了对应的睡眠状态。  

wait_event_timeout 传入的 cmd 参数为：__ret = schedule_timeout(__ret),而 __ret 原为用户传入的 timeout 参数。  

源码实现如下：

```
static void process_timeout(unsigned long __data)
{
	wake_up_process((struct task_struct *)__data);
}

signed long __sched schedule_timeout(signed long timeout)
{
	struct timer_list timer;
	unsigned long expire;

	//特殊处理

	expire = timeout + jiffies;

	setup_timer_on_stack(&timer, process_timeout, (unsigned long)current);
	__mod_timer(&timer, expire, false);
	schedule();

	//睡眠断点
	del_singleshot_timer_sync(&timer);

	destroy_timer_on_stack(&timer);

	timeout = expire - jiffies;

 out:
	return timeout < 0 ? 0 : timeout;
}
```
timeout 以 jiffies 为单位，schedule_timeout 这个接口的实现并不复杂，设置一个定时器，然后进入睡眠。在定时回调函数中调用 wake_up_process 接口来唤醒进程。  

因为内核中的定时器由内核管理，与任何进程都没有关系，所以进程进入睡眠之后定时器依旧可以触发。  

而 wake_up_process 是内核中常用的唤醒指定进程/内核线程的接口，当程序执行到 schedule 时，由于在 prepare_to_wait_event 中设置进程状态为 TASK_UNINTERRUPTIBLE，该进程就进入到了不可中断的睡眠状态。   

当定时器超时并调用回调函数时，进程被唤醒，继续在睡眠的断点处执行，将删除定时器，此时的 expire - jiffies 等于 0 ，所以该函数返回 0 。

再回过头来看整个 ___wait_event 的实现：

```
#define ___wait_event(wq_head, condition, state, exclusive, ret, cmd)		\
({										\
	__label__ __out;							\
	struct wait_queue_entry __wq_entry;					\
	long __ret = ret;	/* explicit shadow */				\
										\
	init_wait_entry(&__wq_entry, exclusive ? WQ_FLAG_EXCLUSIVE : 0);	\
	for (;;) {								\
		long __int = prepare_to_wait_event(&wq_head, &__wq_entry, state);\
										\
		//调用 ___wait_event 的父宏传入的是 ___wait_cond_timeout(condition)，为了讲解的清晰起见，我将这里的 condition 替换成了 ___wait_cond_timeout(condition)，毕竟宏的处理就是文本替换。  
		if (___wait_cond_timeout(condition))				\
			break;							\
										\
		if (___wait_is_interruptible(state) && __int) {			\
			__ret = __int;						\
			goto __out;						\
		}								\
										\
		//cmd;
		__ret = schedule_timeout(__ret)						\
	}									\
	finish_wait(&wq_head, &__wq_entry);					\
__out:	__ret;									\
})
```
在定时器超时的情况下，schedule_timeout 返回 0 并赋值给 __ret，此时因为程序流程还处于死循环中，重复 foor(;;)中的内容，我们看到第二部分 ___wait_cond_timeout(condition)  

```
#define ___wait_cond_timeout(condition)						\
({										\
	bool __cond = (condition);						\
	if (__cond && !__ret)							\
		__ret = 1;							\
	__cond || !__ret;							\
})
```
__ret 的的值为 0 ，所以该宏的返回值为 1 ，

```
if (___wait_cond_timeout(condition))				\
			break;	
```
这条判断语句成立，break 退出循环，执行 finish_wait，也就是当前进程被唤醒，继续执行唤醒之后的程序。  

这就是整个 wait_event_timeout 超时唤醒的流程。  

## 小结
对比 wait_event、wait_event_timeout、wait_event_interruptible 三个接口的实现，它们有以下特点：

三个接口最后都是调用 ___wait_event 接口，只是传入的参数不同，wait_event 、wait_event_timeout 传入的进程状态(state)为 TASK_UNINTERRUPTIBLE,表示在睡眠过程中不处理信号，

而 wait_event_interruptible 传入的进程状态为 TASK_INTERRUPTIBLE，因为信号发送接口会唤醒进程，所以将执行到 ___wait_event->prepare_to_wait_event->signal_pending_state 来处理信号，并唤醒进程。  

wait_event_timeout 传入一个 timeout 参数，将会执行 schedule_timeout 来处理超时情况，超时即返回。  


## 伪唤醒和真唤醒
可以从源代码中看出，___wait_event 的实现由一个死循环构成，也就是说：即使当前睡眠的进程被唤醒，它也会在死循环中继续执行，除非满足某些特定的条件，调用 break 或者 goto 跳出循环。   

伪唤醒和真唤醒是博主自创的词，伪唤醒指的是该进程从睡眠状态中由于各种原因被唤醒，但是仍旧在循环中继续执行 prepare_to_wait_event、___wait_is_interruptible、schedule，再次进入睡眠，比较典型的就是  wait_event 接口对信号的处理，信号发送函数的确会将该进程唤醒，但是又会因条件不足再次陷入睡眠。

真唤醒就是进程满足了某些条件，最常见的就是 condition 为真，或者 wait_event_timeout 中超时，又或者是 wait_event_interruptible 接收到信号。   

内核中最常用的唤醒等待队列上进程的接口是 ：wake_up,wake_up_interruptible,wake_up_all。这三个接口的作用在上一章有解释，这里就不再赘述，我们主要来看看它们的源代码实现。  

## wake_up 系列函数
最常见的 wake_up 系列函数有三个：wake_up,wake_up_interruptible,wake_up_all.  

wake_up 系列函数以等待队列头为参数，实现正如其名：唤醒等待队列上的进程，但是根据上文中对于 wait_event 系列函数的分析，调用 wake_up 唤醒等待队列上的进程，并不一定就代表那些进程会被真正唤醒，很可能因为条件不满足又陷入睡眠。   

我们来看看 wake_up 的实现：

```
#define TASK_NORMAL			(TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE)

#define wake_up(x)			__wake_up(x, TASK_NORMAL, 1, NULL)
#define wake_up_all(x)			__wake_up(x, TASK_NORMAL, 0, NULL)
#define wake_up_interruptible(x)	__wake_up(x, TASK_INTERRUPTIBLE, 1, NULL)
```

事实上，如果不涉及到独占进程的使用，即使用上文中提到的 wait_event 系列接口，wake_up 和 wake_up_all 两个接口所起到的作用是一致的。   

而 wake_up_interruptible 只能唤醒由 wait_event_interruptible 接口进入睡眠的进程。  

这三个函数都是调用了 __wake_up 接口：

```

void __wake_up(struct wait_queue_head *wq_head, unsigned int mode,
			int nr_exclusive, void *key)
{
	__wake_up_common_lock(wq_head, mode, nr_exclusive, 0, key);
}

static void __wake_up_common_lock(struct wait_queue_head *wq_head, unsigned int mode,
			int nr_exclusive, int wake_flags, void *key)
{
	unsigned long flags;
	wait_queue_entry_t bookmark;
	```
	nr_exclusive = __wake_up_common(wq_head, mode, nr_exclusive, wake_flags, key, &bookmark);
	```
}

static int __wake_up_common(struct wait_queue_head *wq_head, unsigned int mode,
			int nr_exclusive, int wake_flags, void *key,
			wait_queue_entry_t *bookmark)
{
	...
	list_for_each_entry_safe_from(curr, next, &wq_head->head, entry) {
		unsigned flags = curr->flags;
		int ret;
		...
		ret = curr->func(curr, mode, wake_flags, key);
		...
	}
}

```
根据源码可以看到：__wake_up 调用 __wake_up_common_lock，接着调用 __wake_up_common。  

在 __wake_up_common 中，对等待队列中的每一个节点成员，调用唤醒回调函数，也就是进程在使用 wait_event 函数时系统传入的回调函数(在 ___wait_event->init_wait_entry 函数中被赋值)：

```
void init_wait_entry(struct wait_queue_entry *wq_entry, int flags)
{
	wq_entry->flags = flags;
	wq_entry->private = current;
	wq_entry->func = autoremove_wake_function;
	INIT_LIST_HEAD(&wq_entry->entry);
}
```

我们继续看这个唤醒回调函数 autoremove_wake_function：

```
int autoremove_wake_function(struct wait_queue_entry *wq_entry, unsigned mode, int sync, void *key)
{
	int ret = default_wake_function(wq_entry, mode, sync, key);
}
int default_wake_function(wait_queue_entry_t *curr, unsigned mode, int wake_flags,
			  void *key)
{
	return try_to_wake_up(curr->private, mode, wake_flags);
}

```
try_to_wake_up 函数接口将唤醒指定的进程，在 try_to_wake_up 怎么区分需要唤醒的函数呢？

```
static int
try_to_wake_up(struct task_struct *p, unsigned int state, int wake_flags)
{
	...
	if (!(p->state & state))
		goto out;
	...
}
```
其中，p 是传入的需要唤醒的目标进程的 task_struct，在使用 wait_event 系列函数时，我们根据不同的接口分别设置了 p->state=TASK_INTERRUPTIBLE 或者 p->state=TASK_UNINTERRUPTIBLE，在调用该唤醒函数的时候，也根据 wake_up 或者 wake_up_interruptible 传入不同的 state，只有两个 state 相匹配才会进行唤醒。  

所以，wake_up_interruptible 无法唤醒被设置为 TASK_INTERRUPTIBLE 的进程，而 wake_up(_all) 传入的是 TASK_NORMAL 即 (TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE)，所以都可以唤醒。  

至于唤醒之后能不能真正让进程跳出循环向下执行，可以参考上文中对 wait_event 系列接口的实现。  



