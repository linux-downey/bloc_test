# cfs enqueue 和 dequeue
要理解 cfs 调度器,最重要的就是理解其中的三个部分:
* tick 中断的处理,理解在中断中如何对进程的 vruntime 进行更新,以及检查是否需要调度的实现细节
* 进程实体的 enqueue 和 dequeue,了解一个进程在什么情况下会被加入到就绪队列和移出就绪队列,理解 enqueue 和 dequeue 的实现细节,了解这些过程中各个数据结构之间的交互
* pick_next_task,选择下一个将要执行的进程,如何选择下一个进程直接体现了 cfs 的调度策略,同时联系前两个部分,梳理 vruntime 的来龙去脉.  

第一部分的讨论可以参考前面的章节(TODO),本章我们主要来讨论第二部分:cfs 中 enqueue 和 dequeue 的实现与使用.讨论实现主要是基于函数的源码分析,同时还需要在内核中找到它们的调用时机.  


## 


ENQUEUE_WAKEUP  和 DEQUEUE_SLEEP 相对，表示是被唤醒或者进入睡眠

DEQUEUE_SAVE、DEQUEUE_MOVE 和 ENQUEUE_RESTORE，这几个标志位在用户使用 __sched_setscheduler 或者设置 nice 值时会被临时从队列中移除，然后又重新加入到队列中。 


最底层的函数为 __enqueue_entity

查看那部分的 commit log 信息
