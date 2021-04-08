# linux 中断子系统 - 工作队列简介以及使用

## 工作队列简介
工作队列(workqueue)是在 2.6 版本引用的下半部机制，相对于 tasklet 和软中断而言，工作队列有一个本质上的区别:它可以运行在进程上下文中。而tasklet 和软中断只能在中断上下文中运行，不支持睡眠和调度，这对某些阻塞式和需要同步执行的任务是不友好的，比如磁盘的访问。  

尽管工作队列可以运行在进程上下文，但是它并不和任何用户进程相关联，它也不能访问任何用户程序空间，因为工作队列是由内核线程执行的，它只会运行在内核中。  


## 工作队列的使用
发展到4.x 甚至 5.x 的内核版本，工作队列的底层实现变得更加复杂(在后续的文章中我们会详细讨论),但是它的应用接口却是始终的简单：初始化和调度执行。  

当然，这个调度执行并不是直接在中断中调用工作函数，而是以设置就绪状态的方式，告诉内核线程，某个工作已经准备好了，它需要在将来的某一时刻执行，这里所说的将来，通常是以 ms 为单位的时间，这个时间具体是多少，没有人知道。  
(注：这里的设置就绪状态并不是设置某个标志位，实际所做的工作是将该工作加入到工作线程然后唤醒线程，设置就绪状态只是一个通用的说法)

但是有一点可以确定的是：通常情况下，一旦工作队列处于就绪状态，工作函数肯定是越快执行越好，这样系统也就拥有更好的响应性能，自 2.6 内核以来，工作队列的优化工作也是主要针对这一部分。   

相关的实现细节我们就在本系列文章的后续章节讨论，接下来我们先来看看工作队列是如何使用的，毕竟这才是一个驱动工程师首要关心的问题。  


## 相关结构体
在这里需要区分两个概念：工作队列和工作。  

工作队列是内核提供的一套实现机制，同时也通常对应内核中的一个软件实体组合，因为现在的工作队列不是一个单纯的队列或链表实现的，为了讨论方便，或者是基于抽象的理念，在讨论应用层面时，我们就把它当成一个队列即可，这一个软件实体组合可以简单地认为由 struct workqueue_struct 描述。  

而工作是实际需要被执行的用户工作，主体是一个工作函数。对于一个驱动工程师而言，需要做的所有事情就是创建工作，然后将其添加到工作队列中等待执行。    

常用的工作分为两种：普通工作和延迟工作，延迟工作仅仅是添加了一个参数：延时时间，单位是 jiffies，工作队列会
在延时时间过后被设置为就绪状态。    

### 普通工作结构体
普通工作的结构体由 struct work_struct 描述：

```
struct work_struct {
	atomic_long_t data;         //相关标志位
	struct list_head entry;     //链表节点
	work_func_t func;           //工作函数
#ifdef CONFIG_LOCKDEP
	struct lockdep_map lockdep_map;   //死锁检测
#endif
};
```

结构成员非常简单：



* atomic_long_t data：看到回调函数和 data 的组合就很容易想到 data 是用户传入的私有数据，但是这里并不是这样实现的，由于工作队列运行在进程上下文，所以可以通过其它方式进行传参，这里的 data 是一组标志位的集合，通常不需要驱动开发者关心。  
* struct list_head entry：工作队列将会以链表的形式管理所有加入的工作，该工作通过这个节点将工作链接到工作队列中。  
* work_func_t func：回调函数，也就是该工作需要执行的工作，函数原型是：typedef void (*work_func_t)(struct work_struct *work);
* struct lockdep_map lockdep_map：lock_dep 是内核中的一个死锁检测模块，用于死锁的检测，这部分不予讨论。  

### 延迟工作结构体
延迟工作的结构体由 struct delayed_work 描述：
```
struct delayed_work {
	struct work_struct work;
	struct timer_list timer;

	/* target workqueue and CPU ->timer uses to queue ->work */
	struct workqueue_struct *wq;
	int cpu;
};
```

延迟工作的结构成员要复杂一些：



* struct work_struct work：普通工作结构体
* struct timer_list timer：内核定时器，用于计时，延时指定的时间就是由内核定时器实现的。  
* struct workqueue_struct *wq：当前工作相关的工作队列。
* int cpu：工作队列机制的实现和 smp (对称多处理器，也可以理解为多核处理器，有多个 CPU 节点) 结构是强相关的，该参数表示当前工作与哪个 cpu 相关。  


## 工作的初始化

内核提供两种类型的接口对工作队列进行初始化:
```
DECLARE_WORK(n, f)
INIT_WORK(_work, _func)
```
分别提供静态和动态的初始化，静态方法的第一个参数只需要提供一个工作名，比如：

```
DECLARE_WORK(my_work, work_callback)
```
系统就会创建一个 struct work_struct my_work 结构并对其成员。

动态初始化需要自定义一个 struct work_struct 类型结构，传入到 INIT_WORK() 中,传入的参数为指针，比如：

```
struct work_struct my_work;
INIT_WORK(&my_work,work_callback);
```

需要注意的是，上述代码如果出现在同一个函数中，这是一个错误实现，因为 my_work 的生命周期限于函数之内，尽管这在某些情况下可以得到正确的结果，但是永远不要这么做，这是编程者常犯的一个错误。相对于此，还有一个常犯的错误：

```
struct work_struct *my_work;
INIT_WORK(my_work,work_callback);
```

在还没有为指针分配内存空间时就使用它，通常编译器会提出警告，某些检查严格的编译器甚至会报错，但是不应该指望编译器来守护你的系统安全。  



而对于延迟工作队列的初始化，也是提供两个接口：

```
DECLARE_DELAYED_WORK(n, f)
INIT_DELAYED_WORK(_work, _func)
```

不难猜到，延迟工作队列的初始化其实就是比普通工作队列多了一项：内核定时器的初始化

```
...
__setup_timer(&(_work)->timer, delayed_work_timer_fn,	\
			      (unsigned long)(_work),			\
			      (_tflags) | TIMER_IRQSAFE);
...
```


对于工作的初始化，比较简单，建议你也去看看源代码。  



## 添加工作队列

将初始化完成的工作添加到工作队列中，工作就会在将来的某个时刻被调度执行，linux 内核已经封装了相应的接口：

```
schedule_work(work);
schedule_delayed_work(delayed_work,jiffies);
```

对于驱动开发者而言，工作的配置就已经完成了，work_struct->func 将会由系统在将来的某个时刻调度执行。   

其中，schedule_work 的实现是这样的：

```
static inline bool schedule_work(struct work_struct *work)
{
	return queue_work(system_wq, work);
}
```

该接口调用了 queue_work，将传入的 work 加入到系统预定义的工作队列 system_wq 中，当然，驱动开发人员也可以直接使用 queue_work() 接口，将当前工作队列添加到其他的工作队列。  

而 schedule_delayed_work 的实现是这样的：

```
static inline bool schedule_delayed_work(struct delayed_work *dwork,
					 unsigned long delay)
{
    //将 work 同样添加到 system_wq 中
	return queue_delayed_work(system_wq, dwork, delay);
}

static inline bool queue_delayed_work(struct workqueue_struct *wq,
				      struct delayed_work *dwork,
				      unsigned long delay)
{
	return queue_delayed_work_on(WORK_CPU_UNBOUND, wq, dwork, delay);
}

bool queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
			   struct delayed_work *dwork, unsigned long delay)
{
	struct work_struct *work = &dwork->work;
    ...
	__queue_delayed_work(cpu, wq, dwork, delay);
    ...
	}
	return ret;
}
static void __queue_delayed_work(int cpu, struct workqueue_struct *wq,
				struct delayed_work *dwork, unsigned long delay)
{
	struct timer_list *timer = &dwork->timer;
	struct work_struct *work = &dwork->work;

	dwork->wq = wq;
	dwork->cpu = cpu;
	timer->expires = jiffies + delay;
    ...
	add_timer(timer);
}
```

可以从源码中看到，经过 queue_delayed_work->queue_delayed_work_on->__queue_delayed_work() 函数之间的层层调用，最后在 __queue_delayed_work 中开启了定时器，而定时器的超时值就是用户传入的 delay 值，同样的，该接口默认地将当前工作队列添加到系统提供的 system_wq 中。    

定时器一旦超时，将会进入到定时器回调函数中，这个回调函数在初始化的时候被设置：

```
void delayed_work_timer_fn(unsigned long __data)
{
	struct delayed_work *dwork = (struct delayed_work *)__data;
	__queue_work(dwork->cpu, dwork->wq, &dwork->work);
}
```

同样是调用了 __queue_work 接口，将 struct work_struct 类型的工作加入到指定工作队列中，至于 __queue_work 的具体实现，因为涉及到比较多的预备知识，我们将在后续的文章中讨论。  


## 工作队列
在上文的工作添加操作中，直接使用系统的接口都会将该工作提交到默认的工作队列 system_wq 中，至于工作队列，内核中系统提供的工作队列分为以下几种：



* 独立于 cpu 的工作队列
* per cpu 的普通工作队列。
* per cpu 的高优先级工作队列
* 可冻结的工作队列
* 节能型的工作队列

除了系统提供的工作队列，用户还可以使用 alloc_workqueue 接口自行创建工作队列，对于这些工作队列的特性，同样将会在后续的文章中继续讨论。


## 等待执行完成
如果需要确定指定的 work 是否执行完成，可以使用内核提供的接口：

```
bool flush_work(struct work_struct *work)
```
这个接口在 work 未完成时会被阻塞直到 work 执行完成，返回 true，但是如果指定的 work 进入了 idle 状态，会返回 false。  

需要注意的是：一个 work 在执行期间可能会被添加到多个工作队列中，flush_work 将会等待所有 work 执行完成。   

针对延迟工作而言，内核接口使用 flush_delayed_work：

```
bool flush_delayed_work(struct delayed_work *dwork)
```

同时需要注意的是，在延迟工作对象上调用 flush 将会取消 delayed_work 的延时时间，也就是会将 delayed_work 立即添加到工作队列并调度执行。  


## 取消工作
另一个类似的接口是：

```
bool cancel_work_sync(struct work_struct *work)
```

这个接口会取消指定的工作，如果该工作已经在运行，该函数将会阻塞直到它完成，对于其它添加到工作队列的工作，将会取消它们。  


## 示例代码
以下是一个简单的 workqueue 示例代码：

```c++
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

MODULE_LICENSE("GPL");


struct wq_priv{
        struct work_struct work;
        struct delayed_work dl_work;
};


static void work_func(struct work_struct *work){
        printk("exec work queue!\n");
}
static void dl_work_func(struct work_struct *work){
        printk("exec delayed work queue!\n");
}
static struct wq_priv priv;

static int __init workqueue_init(void)
{
        printk("hello world!!!\n");

		//初始化 workqueue
        INIT_WORK(&priv.work,work_func);
        INIT_DELAYED_WORK(&priv.dl_work,dl_work_func);
		//调度 workqueue
        if(0 == schedule_work(&priv.work)){
                printk("Failed to run workqueue!\n");
        }
        if(0 == schedule_delayed_work(&priv.dl_work,3*HZ)){
                printk("Failed to run workqueue!\n");
        }
        return 0;
}


static void __exit workqueue_exit(void)
{
	//退出 workqueue
    cancel_work_sync(&priv.work);
    cancel_delayed_work_sync(&priv.dl_work);
}
module_init(workqueue_init);
module_exit(workqueue_exit);
```

将编译后的 .ko 文件加载到内核中，打印信息如下：

```
[17065.062558] exec work queue!
[17068.102369] exec delayed work queue!
```

可以看到，delayed workqueue 的执行时间在 workqueue 之后的 3s，因为设置的 delay 时间就是 3s。  



### 参考

4.14 内核代码

[蜗窝科技：workqueue](http://www.wowotech.net/irq_subsystem/cmwq-intro.html)

[魅族内核团队：workqueue](http://kernel.meizu.com/linux-workqueue.html)

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)








