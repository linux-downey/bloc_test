# linux调度子系统 - 设置进程调度属性
linux 中的进程在创建时(fork)就确定了其调度属性，包括它所属的调度器、优先级，但是这些并不是完全静态的，用户可以通过特定的接口修改它们，这一节来讨论如何对进程的调度属性进行动态地设置，以及这些设置将会引发的调度行为。 



## nice
nice 值是对用户进程优先级的描述，这是 linux 诞生之初就使用的方式，nice 值的范围是 -20~19，值越大优先级越小，这个取值范围着实有点让人摸不着头脑，按照计算机中的编程惯例，要么将负数排除在外，要么将 0 排除在外，因为这样处理起来会显得更优雅、更方便，为什么使用这么一个区间？真相已经淹没在历史的长河中了，而且为了兼容之前的代码，nice 的取值以及用法也就沿袭了下来。  

nice 值只有 40 个取值范围，很明显只对应于用户进程的优先级，在内核的调度行为中，并不使用 nice 值，相对应的使用 static_prio 来替代 nice 作为进程的优先级表示，并且直接提供 nice 与 static_prio 的转换，static_prio 的 100~139 对应 nice 值的 -20~19，其它的对应实时进程的优先级。  

用户程序可以通过 nice 值调整当前进程的优先级，可以通过 man 2 nice 查看 nice 接口：

```c++
int nice(int inc);
```
nice 接收一个 increment 参数，该函数将在当前进程 nice 值的基础上增加 inc 个 nice 值，可以传入负数，表示降低 nice 值，提升进程优先级。  

成功返回新的 nice 值，失败返回 -1，但是有意思的是，设置成功的返回值也可能是 -1(设置完成之后进程的 nice 值为 -1)，如果返回 -1，还需要去判断全局变量 error 来确定是正确返回还是错误返回。  

对于普通用户而言，只能加大 nice 值，也就是降低优先级，如果需要减小进程的 nice 值，需要系统权限，或者需要对当前进程的资源限制进行特殊的配置。  

glibc 中的 nice 函数会发起一个系统调用，对应内核中的 nice syscall:

```c++
SYSCALL_DEFINE1(nice， int， increment)
{
	long nice， retval;

	nice = task_nice(current) + increment;
	nice = clamp_val(nice， MIN_NICE， MAX_NICE);

	retval = security_task_setnice(current， nice);

	set_user_nice(current， nice);
	return 0;
}

```
对于 nice 系统调用而言，第一步是做参数的检查以及权限的检查，毕竟对于没有特殊权限的用户，是不能提升进程的优先级的，如果 nice 值的设置超出了允许的范围(-20~19)，处理方式就是将其设置为对应的极限值，比如当 inc 为 -80 时，nice 系统调用并不会报错，而是设置为它的极限值 -20。  

完成参数检查之后，就需要调用 set_user_nice 函数，从名称可以看出，这是真正执行 nice 值操作的函数，该函数将在下面的程序中解析。

  

## setpriority
nice 的使用方式比较单一，只能针对当前进程进行设置，而不能针对用户组或者用户进行统一设置，setpriority 函数满足了这个需求，与 nice 一样，glibc 中的 setpriority 函数对应内核中的 setpriority 系统调用。  

了解 setpriority 的使用方式，最好的方式同样是通过 man 2 setpriority，它的函数原型为：

```c++
int setpriority(int which， id_t who， int prio);
```
根据 man 手册提供的信息，setpriority 可以为特定进程、进程组以及用户设置优先级，which 和 who 两个参数用来确定需要设置的对象，which 指定是设置哪种类型的对象，有 PRIO_PROCESS/PRIO_PGRP/PRIO_USER，而 who 和 which 相关，用来执行类型对象中的具体对象，如果 which 为 PRIO_PROCESS，那么 who 就应该是指定进程的 PID，同样的，PRIO_PGRP 对应组 ID，PRIO_USER 对应 user id。

而 prio 是需要设置的优先级，和 nice 中的 inc 不一样的是，这个 prio 是本次设置的最终优先级，也就是 prio 为 10 时，目标在设置完成之后优先级就是 10.  

由于 setpriority 支持多种类型对象的优先级设置，对应的系统调用处理也就相对复杂一些，实际上对于进程组和用户这种特殊目标而言，其做法也只是遍历组或者用户中从属的每个进程，再对其依次操作，本质上还是对进程的操作，同时鉴于 setpriority 同样只针对用户进程，而且使用的优先级也是以 nice 值为参考，底层实现其实和 nice 系统调用一样，都是通过 set_user_nice 来对单个或多个目标进程进行操作。  




## set_user_nice
set_user_nice 是内核中用来修改一个进程优先级的接口，接受的参数很简单，第一个是 task 指针，第二个是进程需要设置的 nice 值。对于 nice 系统调用而言，task = current，而对于 setpriority 而言，就不一定了。 
task_nice 的实现并不难：

```c++
nice->set_user_nice:
setpriority->set_one_prio->set_user_nice:

void set_user_nice(struct task_struct *p， long nice)
{
    ...
    rq = task_rq_lock(p， &rf);            ....................1

    queued = task_on_rq_queued(p);
	running = task_current(rq， p);
	if (queued)
		dequeue_task(rq， p， DEQUEUE_SAVE | DEQUEUE_NOCLOCK);
	if (running)
		put_prev_task(rq， p);             .....................2

	p->static_prio = NICE_TO_PRIO(nice);
	set_load_weight(p);
	old_prio = p->prio;
	p->prio = effective_prio(p);
	delta = p->prio - old_prio;            .....................3

	if (queued) {
		enqueue_task(rq， p， ENQUEUE_RESTORE | ENQUEUE_NOCLOCK);
		if (delta < 0 || (delta > 0 && task_running(rq， p)))
			resched_curr(rq);
	}
	if (running)
		set_curr_task(rq， p);                ...................4
    ...
    task_rq_unlock(rq， p， &rf);
}

```
注：上面的代码省略了一些参数检查、错误处理以及其它调度器的处理，以简化流程便于分析。   

注1：在分析内核中的代码时，得养成一个习惯：时刻注意并发问题，对于 rq 而言，尽管它是 percpu 的，但是并不代表它只会被当前 CPU 访问，当我们唤醒一个进程的时候，很有可能需要把进程 pull 到其它 CPU 上，当我们设置一个进程状态时，也完全有可能这个进程正在其它 CPU 上欢快地运行着，每个 CPU 的 rq 都可能正在被其它 CPU 访问，因此，如果你要操作 rq，请务必小心地给它上锁。

注2：从 cfs 调度算法的角度来说，当重新设置一个运行进程或者已就绪进程的优先级时，最方便的做法就是将让该进程以最新设置的参数重新加入调度，也就是先将它从就绪队列中移除，设置完调度参数之后再像一个新进程一样参与调度，而不是基于历史调度情况去做复杂的增减操作，这样操作既简单，也不失公平。 

如果需要操作的进程在队列上，需要将操作的任务先 dequeue_task，而如果需要操作的进程正在运行，需要对这个进程调用 put_prev_task。  

判断进程是否在队列上通过 p->on_rq 标志位，而判断进程是否运行通过 p->on_cpu 标志位(单核下通过rq->curr == p 来判断)，实际上，一个正在运行的进程是同时满足这两个条件的，一个正在运行的进程与单纯只存在于就绪队列上进程的区别在于运行进程不在红黑树上，通常还有一些标志位比如 cfs->curr，p->on_cpu 的区别.   
当然，如果一个进程正在睡眠中，那就简单了，既不需要执行 dequeue_task 也不需要执行 put_prev_task.  

dequeue_task 在之前的文章(TODO)有解析，对于没有正在运行的进程而言，就是将该进程从红黑树上移除(正在运行进程不需要这一步)，更新时间、设置一些标志位比如 on_rq，同时还要递归处理可能存在的组调度的情况。  

如果进程正在运行，就需要调用 put_prev_task，该函数对调用 task 对应调度器类的 put_prev_task 回调函数，对应 cfs 调度器的 put_prev_task_fair，该函数通常在 schedule 函数中被调用，用于将 cfs 上当前运行的进程设置为非运行状态，如果该进程 p->on_rq 为 1，就把它重新加入到红黑树，否则不加，同时设置 cfs->curr 为 NULL，需要注意的是:此时的 p->on_cpu 和 p->on_rq 并没有被置为 0，设置调度参数属于特定调度器的工作，而 p->on_cpu 和 p->on_rq 属于全局的核心调度部分。在 cfs 调度器的眼中，该进程实体已经没有运行且移除队列了，而在当前或者其它的 CPU 眼中，该进程还是处于运行状态.    

需要注意，执行 put_prev_task_fair 并不代表当前进程就被切出去了，它只是将相应的标志位进行了设置，而进程还在欢快地执行，而且在这期间对应的 CPU 是不会发生内核抢占的，因为当前操作已经被 task_rq_lock 保护，而 task_rq_lock 由 spinlock 实现，同时 task_rq_lock 还关闭了本地中断.  

注3:当目标进程执行完 dequeue 或者 put_prev_task 之后，cfs 就可以执行对进程调度参数的调整了，在 cfs 调度器中，用户空间的 nice 值直接对应进程的 static_prio ，然后通过 static_prio 设置 p->se 的 load weight，在实体的调度中，load weight 是最主要的参考值.而 prio 和 normal_prio 对于 cfs 的主要调度过程重要性不高.主要是因为在某些场合需要对所有调度器类进行统一操作时，使用 prio 和 normal_prio 作为参考.  

调度参数的设置还会计算目标进程原本的优先级与当前设置的优先级之间的差值，如果优先级升高了，需要设置抢占标志位，如果优先级降低了，而且目标进程正在运行，也需要设置重新调度，注意:即使设置了重新调度标志，因为目前处于关中断的状态，并不会真正触发调度的执行.  

注4:在设置进程调度参数之前，对进程执行了 dequeue 或者 put，这种操作并不是由于真正的切换需求，因此，在设置完调度参数之后需要将进程重新入队或者重新设置为当前运行进程，具体的执行函数为 enqueue_task 和 set_curr_task，和上面的 dequeue_task 和 put_prev_task 执行相反的操作.这样，也就圆满地完成了进程调度参数的设置.   

最后，调用 task_rq_unlock 解锁目标 CPU 的 rq，等待的中断会被触发执行，所设置的抢占标志也会触发内核抢占，内核又重新活跃了起来.    




## sched_setscheduler
上面提到的两个函数用来设置非实时进程的调度参数，主要是针对 cfs 调度器，而 sched_setscheduler 用来设置进程的调度策略以及相应的优先级.   

进程的调度策略由 policy 表示，主要为以下几种:
* SCHED_NORMAL:正常的非实时进程
* SCHED_BATCH:批处理进程
* SCHED_IDLE:针对优先级非常低的进程
这三种调度策略对应的进程属于非实时进程，也就是由 cfs 调度器管理.   

实际上，对一个 nice 为 0 的进程分别设置这三种调度策略，并不会在进程的优先级上有任何区别，它们更多地被使用在分时类型的调度器上，但是随着 cfs 调度器的引入，这三种调度策略之间的区别逐渐被弱化了，只会在一些特殊的情况下对这三种策略作区分处理.  

对于其它调度器的调度策略为:
* SCHED_FIFO:先进先出调度类型的实时进程，先进入的进程享有持续执行的权利
* SCHED_RR:分时调度类型的实时进程

实际上，如果使用 man sched_setscheduler 获取 sched_setscheduler 的相关信息，对于调度策略的定义是有区别的，这是因为 posix 接口通常是固定的，而接口对应的内核实现通常是变化的，这种静态与动态之间就会造成一些差异.  

如果你经常查看系统调用相关的代码，就会发现很多 linux 中为了兼容之前的接口而写出一些不得已而为之的丑陋代码.比如上面 nice 接口的设计，错误返回竟然和正常返回值有重叠的部分，看来不管是 windows 还是 linux，向前/向后兼容都是优雅设计的绊脚石，但是出于商业发展，这又是必须的.      

policy 除了用来设置进程的调度策略外，还可以设置 SCHED_RESET_ON_FORK 标志位，这个标志位曾经在进程新建调度设置章节(TODO)有提到，进程如果设置了该标志，其 fork 出的子进程默认为 nice 为 0 的非实时进程，这个标志通常用来对实时进程进行设置.  

sched_setscheduler 对应的内核函数原型如下:

```c++
SYSCALL_DEFINE3(sched_setscheduler， pid_t， pid， int， policy， struct sched_param __user *， param)
```
函数接受三个参数，第一个 pid 不用过多解释，表示目标进程的 pid，0 表示发起系统调用的进程(或线程)，policy 见上文，第三个参数表示参数，目前的参数只有一个:sched_priority.  

sched_priority 和上面 setpriority 参数不一样，由于 sched_setscheduler 是针对所有调度器的，自然不会只照顾 cfs 调度器，当设置的调度策略为非实时进程时，sched_priority 只能为 0，也就是不能调节非实时进程的优先级，而对于实时进程而言(SCHED_FIFO和SCHED_RR)，setpriority 参数为 1-99，表示实时进程的优先级.  

除了 sched_setscheduler 之外，还有其它的接口比如 sched_setparam，sched_setattr 也可以对调度策略和优先级进行设置，各个系统调用的侧重点不同，不过对于非实时进程而言，其最终执行的过程和 set_user_nice 函数差不多，也就是设置之前保证进程 dequeue，设置之后再 enqueue 重新加入调度，就不深入讨论了.  





