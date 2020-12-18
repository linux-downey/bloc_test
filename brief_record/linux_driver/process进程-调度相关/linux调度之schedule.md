# linux 调度之 schedule()
schedule() 是 linux 调度器中最重要的一个函数,就像 fork
函数一样优雅,它没有参数,没有返回值,却实现了内核中最重要的功能,当需要执行实际的调度时,直接调用 shedule(),进程就这样神奇地停止了,而另一个新的进程占据了 CPU.    

在什么情况下需要调度?  
在 scheduler_tick 中,唤醒进程时,重新设置调度参数这些情况下都会检查是否出现抢占调度,但是像 scheduler_tick 中或者中断中唤醒高优先级进程时,这是在中断上下文中,是不能直接执行调度行为的,因此只能设置抢占标志,然后在中断返回的时候再调用 schedule() 函数执行调度.   

上面说的这几种都是被动调度的情况,也就是进程并不是自愿执行调度,而是被抢走了 CPU 执行权,与被动调度相对应的还有主动调度,也就是进程主动放弃 CPU,通常对应的代码为:

```c++
...
set_current_state(TASK_INTERRUPTIBLE);
schedule()
...
```

在 schedle 函数的执行中,current 进程进入睡眠,而一个新的进程被运行,当当前进程被唤醒并得到执行权时,又接着 schedule 后面的代码运行,非常简单的实现,完美地屏蔽了进程切换的内部实现,提供了最简单的接口,这一章我们就来翻开这些被屏蔽的细节,看看 linux 中进程的调度到底是怎么执行的.   

## schedule 源码实现
schedule 函数定义在 kernel/sched/core.c 中,源码如下:

```c++
void __sched schedule(void)
{
	struct task_struct *tsk = current;
    ...
	do {
		preempt_disable();
		__schedule(false);
		sched_preempt_enable_no_resched();
	} while (need_resched());
}
```
schedule() 函数只是个外层的封装,实际调用的还是 __schedule() 函数, __schedule() 接受一个参数,该参数为 bool 型,false 表示非抢占,自愿调度,而 true 则相反.   

在执行整个系统调度的过程中,需要关闭抢占,这也很好理解,内核的抢占本身也是为了执行调度,现在本身就已经在调度了,如果不关抢占,递归地执行进程调度怎么看都是一件没必要的事.  

当然,在调度过程完成之后,也就是 __schedule 返回之后,这个过程中可能会被设置抢占标志,这时候还是需要重新执行调度的.  

但是这里有一个很有意思的事:在执行调度函数 __schedule 的过程中,实际上在某个点已经切换出去了,而当前进程的 __schedule 函数返回的时候其实是当前进程被重新执行的时候,可以简单地理解为,如果发生了实际的进程切换,在 __schedule 函数内实际上经历了一次进程从切出到再次切入,经历的时间可能是几百毫秒或者更长.  

因此,实际上,在当前进程中禁止的抢占,而使能抢占的 sched_preempt_enable_no_resched 函数却是在另一个进程上执行的.好在所有的进程都是通过 schedule() 进行进程切换的,也就保证了 disable 和 enable 总是成对的.    


## __schedule()
__schedule 的实现大概可以分为四个部分：
* 针对当前进程的处理
* 选择下一个需要执行的进程
* 执行切换工作
* 收尾工作

整个 __schedule 函数比较长，也分成这四个部分进行分析。 

### 针对当前进程的处理











