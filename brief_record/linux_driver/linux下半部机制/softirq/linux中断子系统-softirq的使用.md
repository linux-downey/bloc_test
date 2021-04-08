# linux 下半部机制 - softirq使用
在中断的下半部机制中，软中断是执行效率最快的，同时，相对来说对于开发者也是最麻烦的，它的麻烦在于两个因素：



* 同一个软中断支持在不同的 cpu 上并发执行，这也就导致了软中断执行的代码需要考虑 SMP 下的并发，实现上要更复杂。
* 软中断不支持动态的定义，只能将软中断静态地编译到内核镜像中，而不能使用外部模块加载的方式



它的执行效率也体现在两点上：



* 因为支持 cpu 上并发执行，所以通常情况下不需要等待(tasklet无法并发执行，且有其他限制)，但是硬中断能抢占它执行。  
* 通常情况下软中断执行在中断上下文中，硬中断结束之后会立马执行软中断，为什么说是通常情况下运行在中断上下文而不是一定运行在中断上下文？这是因为在特殊情况下，软中断也会由内核线程来实现(后文详述)。  




## softirq 的使用
linux 整个中断子系统的应用接口设计得非常简洁，下半部的 API 实现就是其中的代表，和 workqueue 一样，softirq 的使用就是两部分：



* 初始化
* 执行



### 初始化
正如上文所说，softirq 的初始化是静态定义的，它的定义在 include/linux/interrupt.h 中：

```c++
/* PLEASE, avoid to allocate new softirqs, if you need not _really_ high
   frequency threaded job scheduling. For almost all the purposes
   tasklets are more than enough. F.e. all serial device BHs et
   al. should be converted to tasklets, not to softirqs.
 */
enum
{
	HI_SOFTIRQ=0,
	TIMER_SOFTIRQ,
	NET_TX_SOFTIRQ,
	NET_RX_SOFTIRQ,
	BLOCK_SOFTIRQ,
	IRQ_POLL_SOFTIRQ,
	TASKLET_SOFTIRQ,
	SCHED_SOFTIRQ,
	HRTIMER_SOFTIRQ, /* Unused, but kept as tools rely on the
			    numbering. Sigh! */
	RCU_SOFTIRQ,    /* Preferable RCU should always be the last softirq */

	NR_SOFTIRQS
};
```
在关注它的成员实现之前我们可以先看看它的注释：如果你的工作不是非常高频地被执行，请尽量不要定义 softirq，使用 tasklet 来代替处理。   

显然，内核是不建议开发者自定义使用软中断接口的，尽管它的执行效率确实比 tasklet 高，但是在大多数情况下，使用这两者的区别并不大，在后续的 softirq 和 tasklet 原理分析时你会知道为什么。    

话归正传，内核中默认定义了几种软中断实现，分别有：高优先级tasklet、定时器、网络收发、块设备、普通tasklet、高精度定时器、RCU。  

如果我们想要添加一个 softirq 的实现，需要在 NR_SOFTIRQS 上添加一个 enum 成员，因为这是 enum 结构，所以 NR_SOFTIRQS 这个值就是当前系统中存在的 softirq 的数量。   


接着，需要定义一个触发 softirq 时的执行函数 open_softirq，这个函数的定义可以在 softirq_init 中，也可以定义在后续的驱动程序初始化中，函数接口为：

```
void open_softirq(int nr, void (*action)(struct softirq_action *))
{
	softirq_vec[nr].action = action;
}
```

第一个参数 nr 是之前的 enum 结构中定义的枚举值，第二个参数 action 是一个函数指针，指向需要执行的函数。   



比如 tasklet 的添加：

```
open_softirq(TASKLET_SOFTIRQ, tasklet_action);
```
顺便说一句，tasklet 的实现是基于软中断的。   



### softirq 的执行
想要执行一个 softirq ，需要使用的接口是 raise_softirq：

```
void raise_softirq(unsigned int nr);
```
接口也是非常简单，只要传入在定义时的枚举值，做完这两件事，就只需要等待 softirq 被执行即可。   




## softirq 知识问答
### softirq 什么时候会被执行
作为中断的下半部机制，softirq 会在中断执行完成之后被立即调用执行，具体的实现在 irq_exit() 函数中，从函数名可以看出，这是在退出硬件中断时调用的。  

因为 softirq 是可以被硬中断抢占执行的，可能存在这种情况：在执行 softirq 时，被硬件中断抢占执行，在硬件中断退出时在 irq_exit() 函数中会不会重复执行 softirq？
答案是不会，当原本就处于 softirq 执行环境下时，不会重新进入软中断执行，而是在中断返回的时候返回到上次软中断被打断的现场继续执行软中断。    

在编写驱动程序时，出于同步的考虑，内核中可以使用 local_bh_disable 接口禁用下半部，而在调用 local_bh_enable 重新使能下半部的时候，软中断也会被触发调用。  


### softirq 是不是一定运行在中断上下文
当系统比较繁忙或者是中断比较频繁时，发生中断，中断做一些硬件处理，比如清除 mask 以重新接收中断、或者将数据拷贝到内核，其他的事务交由软中断处理，中断退出，执行软中断。

在软中断刚处理完或者还没处理完的时候又进来一个相同的中断执行，如果这种中断非常频繁，硬中断和软中断的执行将会导致普通进程分配不到合理的执行时间，尤其是对于用户交互的进程而言，这是很糟糕的。   

所以在这种情况下，部分软中断会被移交到内核线程中执行，将其调度优先级降低。在这种情况下，软中断运行在进程上下文中，但是千万不要以为软中断可能运行在进程上下文，你就可以在执行函数中添加可能造成阻塞的程序，这是绝对不允许的。  



### SMP 架构上软中断的执行
和 workqueue 一样，软中断的执行也是 cpu 相关的，也就是当 raise_softirq 被调用时，对应的软中断执行函数将会被添加当前 cpu 的待执行列表中，也只会在当前 cpu 上执行。   

遗憾的是，尽管是 percpu 方式的软中断处理，还是需要考虑多核下的数据保护，这是因为同一个软中断可能在多 cpu 上并发执行。  

同样是一个简单的例子：CPU0 触发中断，中断中 raise 了一个软中断，退出中断时将会执行软中断，这时又一个同样的硬件中断进来，被分配到了 CPU1，CPU1 的中断处理程序同样会 raise 这个软中断， CPU0 的软中断因为某些原因被延迟执行了，或者 CPU0 还没执行完，CPU1 开始执行，这时候两个软中断就相当于在不同的 cpu 上并发执行。  




## 代码示例

接下来演示如何在系统中添加一个新的 softirq，softirq 的添加是一个比较麻烦的操作。  

### 修改内核

首先，需要在系统中静态地添加一个新的 softirq 枚举成员，该成员在 include/linux/interrupt.h 中添加：

```
enum
{
	HI_SOFTIRQ=0,
	TIMER_SOFTIRQ,
	NET_TX_SOFTIRQ,
	NET_RX_SOFTIRQ,
	BLOCK_SOFTIRQ,
	IRQ_POLL_SOFTIRQ,
	TASKLET_SOFTIRQ,
	SCHED_SOFTIRQ,
	HRTIMER_SOFTIRQ, 
	RCU_SOFTIRQ,    

	TEST_SOFTIRQ,    //新添加的 softirq

	NR_SOFTIRQS
};
```

默认情况下，softirq 的两个操作接口 oepn_softirq 和 raise_softirq 是没有导出符号的，需要将这两个函数的符号导出，外部接口才能使用，具体操作是分别在 open_softirq 和 raise_softirq 函数实现后添加导出变量的宏：

```c++
EXPORT_SYMBOL(open_softirq);  //添加到 open_softirq 函数实现下
EXPORT_SYMBOL(raise_softirq); //添加到 raise_softirq 函数实现下
```
这两个函数的定义在 kernel/softirq.c 中。  

做完这两个操作之后，重新编译内核并将新编译的内核替换。   



### 编写驱动程序

在修改完内核之后，就可以编写外部驱动程序测试新添加的 softirq 了：

```c++
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");

static void test_softirq_action(struct softirq_action *a)
{
        printk("Test softirq excuted!\n");
}

static int __init msoftirq_init(void)
{
		//注册 softirq
        open_softirq(TEST_SOFTIRQ, test_softirq_action);
		//触发 softirq
        raise_softirq(TEST_SOFTIRQ);

        return 0;
}

static void __exit msoftirq_exit(void)
{
}

module_init(msoftirq_init);
module_exit(msoftirq_exit);

```
编译并加载该 softirq 代码，使用 dmesg | tail 指令就可以看到 softirq 输出的log：

```
[  157.500252] Test softirq excuted!
```

需要注意的是，这里使用 printk 函数仅仅是为了方便查看调试结果，实际上 softirq 中是不建议直接使用 printk 函数的，printk 函数并不是可重入函数而且执行时间较长，不适合在软中断中执行。  





### 参考

4.14 内核代码



[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)







